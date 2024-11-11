/********************************************************************************
* INTEL CONFIDENTIAL
* Copyright (C) 2023 Intel Corporation
*
* This software and the related documents are Intel copyrighted materials,
* and your use of them is governed by the express license under
* which they were provided to you ("License").Unless the License
* provides otherwise, you may not use, modify, copy, publish, distribute, disclose or
* transmit this software or the related documents without Intel's prior written permission.
*
* This software and the related documents are provided as is,
* with no express or implied warranties, other than those that are expressly stated in the License.
*******************************************************************************/

#include "threading/ivsr_thread_executor.hpp"
#include "ov_engine.hpp"

#include <atomic>
#include <cassert>
#include <climits>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace IVSRThread {
struct IVSRThreadExecutor::Impl {
    struct Stream {
        explicit Stream(Impl* impl) : _impl(impl) {
            {
                std::lock_guard<std::mutex> lock{_impl->_streamIdMutex};
                if (_impl->_streamIdQueue.empty()) {
                    _streamId = _impl->_streamId++;
                } else {
                    _streamId = _impl->_streamIdQueue.front();
                    _impl->_streamIdQueue.pop();
                }
            }
            

        }
        ~Stream() {
            {
                std::lock_guard<std::mutex> lock{_impl->_streamIdMutex};
                _impl->_streamIdQueue.push(_streamId);
            }

        }

        Impl* _impl = nullptr;
        int _streamId = 0;
        int _numaNodeId = 0;
        bool _execute = false;
        std::queue<Task> _taskQueue;
        
    };

    explicit Impl(const Config& config, engine<ov_engine>* engine)
        : _config{config},
        _engine(engine),
          _streams([this] {
              return std::make_shared<Impl::Stream>(this);
          }) {
        for (auto streamId = 0; streamId < _config._threads; ++streamId) {
            _threads.emplace_back([this, streamId] {
                for (bool stopped = false; !stopped;) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
      			_queueCondVar.wait(lock, [&] {
                            return !_taskQueue.empty() || (stopped = _isStopped);
                        });
                        if (!_taskQueue.empty()) {
                            task = _taskQueue.front();
                            _taskQueue.pop();
                        }
                    }
                    if (task) {
#ifdef ENABLE_LOG
                        std::cout << "[Trace]: " << "Thread " << std::this_thread::get_id() << " get task and execute it" << std::endl;
#endif
                        Execute(task, *(_streams.local()));
                    }
                }
            });
        }
    }

    void Enqueue(Task task) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _taskQueue.emplace(task);
	    _startTime = std::min(Time::now(), _startTime);
        }
        _queueCondVar.notify_one();
#ifdef ENABLE_LOG
        std::cout << "[Trace]: " << "Enqueue Task into queue and notify 1 / " << _taskQueue.size() << std::endl;
#endif 
    }

    void Execute(const Task& task, Stream& stream) {
        _engine->run(task);
    }

    Task CreateTask(char* inBuf, char* outBuf, InferFlag flag) {
        Task task = std::make_shared<InferTask>(inBuf, outBuf,  std::bind(&IVSRThread::IVSRThreadExecutor::Impl::competition_call_back, this), flag);
        return task;
    }
    void Defer(Task task) {
        auto& stream = *(_streams.local());
        stream._taskQueue.push(std::move(task));
        if (!stream._execute) {
            stream._execute = true;
            try {
                while (!stream._taskQueue.empty()) {
                    Execute(stream._taskQueue.front(), stream);
                    stream._taskQueue.pop();
                }
            } catch (...) {
            }
            stream._execute = false;
        }
    }

    void sync(int size) {
	std::unique_lock<std::mutex> lock(_mutex);
    _taskCondVar.wait(lock,[&] {
                             return (_cb_counter == size);
        });

    }

    void reset() {
        std::unique_lock<std::mutex> lock(_mutex);
        _cb_counter = 0;
    }

    void competition_call_back() {
        std::unique_lock<std::mutex> lock(_mutex);
      	_cb_counter++;
	      _endTime = std::max(Time::now(), _endTime);
        _taskCondVar.notify_one();
    }
    
    double get_duration_in_milliseconds() {
        return std::chrono::duration_cast<ns>(_endTime - _startTime).count() * 0.000001;
    }

    Config _config;
    std::mutex _streamIdMutex;
    int _streamId = 0;
    std::queue<int> _streamIdQueue;
    std::vector<std::thread> _threads;
    std::mutex _mutex;
    std::condition_variable _queueCondVar;
    std::condition_variable _taskCondVar;
    int _cb_counter = 0;
    std::queue<Task> _taskQueue;
    bool _isStopped = false;
    ThreadLocal<std::shared_ptr<Stream>> _streams; 
    engine<ov_engine>* _engine;
    Time::time_point _startTime = Time::time_point::max();
    Time::time_point _endTime = Time::time_point::min();    
};


IVSRThreadExecutor::IVSRThreadExecutor(const Config& config, engine<ov_engine>* engine) : _impl{new Impl{config, engine}} {}

IVSRThreadExecutor::~IVSRThreadExecutor() {
    {
        std::lock_guard<std::mutex> lock(_impl->_mutex);
        _impl->_isStopped = true;
    }
    _impl->_queueCondVar.notify_all();
    for (auto& thread : _impl->_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void IVSRThreadExecutor::Execute(Task task) {
    _impl->Defer(task);
}

void IVSRThreadExecutor::Enqueue(Task task) {
    if (0 == _impl->_config._threads) {
        _impl->Defer(task);
    } else {
        _impl->Enqueue(task);
    }
}

Task IVSRThreadExecutor::CreateTask(char* inBuf, char* outBuf, InferFlag flag) {
    Task task = _impl->CreateTask(inBuf, outBuf, flag);
    return task;
}

void IVSRThreadExecutor::wait_all(int patchSize) {
    _impl->sync(patchSize);
    _impl->reset();
}

double IVSRThreadExecutor::get_duration_in_milliseconds() {
        return _impl->get_duration_in_milliseconds();
}
}  // namespace IVSRThread
