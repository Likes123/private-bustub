//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// rwmutex.h
//
// Identification: src/include/common/rwlatch.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <climits>
#include <condition_variable>  // NOLINT
#include <mutex>               // NOLINT

#include "common/macros.h"

namespace bustub {

/**
 * Reader-Writer latch backed by std::mutex.
 */
class ReaderWriterLatch {
  using mutex_t = std::mutex;
  using cond_t = std::condition_variable;
  static const uint32_t MAX_READERS = UINT_MAX;

 public:
  ReaderWriterLatch() = default;
  ~ReaderWriterLatch() { std::lock_guard<mutex_t> guard(mutex_); }

  DISALLOW_COPY(ReaderWriterLatch);

  /**
   * Acquire a write latch.
   */
   //lock_guard和unique_lock的相同点是RAII，避免资源泄漏
  //但是unique_lock独有的地方在于可以在RAII释放资源前unlock和lock，lock_guard不行
  //有序条件变变量需要wait中unlock，只能使用unique_lock，事实上wait就只支持unique_lock
  void WLock() {
    //思路是看while中等待是事件，看wait对应的notify，单单看reader_和writer_的命名让人困扰
    std::unique_lock<mutex_t> latch(mutex_);
    //写入的前提是既没有人读，也没有人写
    //判断对应的事件发生没有，使用while而不是if，wait被唤醒，但是不一定马上能获取到锁，
    //如果写者又获取到锁，并进行写入取，对应的writer_entered_将一直循环，确保安全
    //等待没有人写
    while (writer_entered_) {
      //等待读者修改等待的事件后发出信号，wait中会释放latch，wait唤醒重新锁住latch后返回
      reader_.wait(latch);
    }
    writer_entered_ = true;
    //等待所有的读者都读完
    while (reader_count_ > 0) {
      writer_.wait(latch);
    }
  }

  /**
   * Release a write latch.
   */
  void WUnlock() {
    std::lock_guard<mutex_t> guard(mutex_);
    //唤醒所有的writer和reader
    writer_entered_ = false;
    reader_.notify_all();
  }

  /**
   * Acquire a read latch.
   */
  void RLock() {
    std::unique_lock<mutex_t> latch(mutex_);
    //当没有人写且读者数小于最大读者数，可以出循环
    while (writer_entered_ || reader_count_ == MAX_READERS) {
      reader_.wait(latch);
    }
    reader_count_++;
  }

  /**
   * Release a read latch.
   */
  void RUnlock() {
    std::lock_guard<mutex_t> guard(mutex_);
    reader_count_--;
    if (writer_entered_) {//有写者在等
      //确保没有人在读，没有此句逻辑上不会有问题，因为writer在while循环中会再判断一次
      //此判断主要是提升效率
      if (reader_count_ == 0) {
        writer_.notify_one();//只需要唤醒一个，避免惊群
      }
    } else {
      if (reader_count_ == MAX_READERS - 1) {//由于到了最大读者数，有读者在等
        reader_.notify_one();
      }
    }
  }

 private:
  mutex_t mutex_;
  cond_t writer_;
  cond_t reader_;
  uint32_t reader_count_{0};
  bool writer_entered_{false};
};

}  // namespace bustub
