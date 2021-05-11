/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2010, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#include <gtest/gtest.h>

#include "rosrt/malloc_wrappers.h"
#include "ros/atomic.h"
#include <ros/time.h>
#include <ros/console.h>

#include <boost/thread.hpp>

#if __unix__ && !APPLE
#include <dlfcn.h>
#endif

using namespace rosrt;
using namespace ros;

void allocateThread(const atomic<bool>& done)
{
  while (!done.load())
  {
    void* mem = malloc(500);
    free(mem);
    ros::WallDuration(0.001).sleep();
  }
}

TEST(MallocWrappers, statsMainThread)
{
  atomic<bool> done(false);
  boost::thread t(boost::bind(allocateThread, boost::ref(done)));

  resetThreadAllocInfo();

  for (uint32_t i = 1; i <= 1000; ++i)
  {
    void* mem = malloc(500);
    free(mem);
    ros::WallDuration(0.001).sleep();

    AllocInfo info = getThreadAllocInfo();
    ASSERT_EQ(info.mallocs, i);
    ASSERT_EQ(info.frees, i);
    ASSERT_EQ(info.total_ops, i * 2);
  }

  done.store(true);
  t.join();
}

void statsThread(atomic<bool>& failed)
{
  resetThreadAllocInfo();

  for (uint32_t i = 1; i <= 1000; ++i)
  {
    void* mem = malloc(500);
    free(mem);
    ros::WallDuration(0.001).sleep();

    AllocInfo info = getThreadAllocInfo();
    if (info.mallocs != i)
    {
      ROS_ERROR_STREAM("mallocs is " << info.mallocs << " should be " << i);
      failed.store(true);
      return;
    }

    if (info.frees != i)
    {
      ROS_ERROR_STREAM("mallocs is " << info.frees << " should be " << i);
      failed.store(true);
      return;
    }
  }
}

TEST(MallocWrappers, statsNewThread)
{
  atomic<bool> failed(false);
  boost::thread t(boost::bind(statsThread, boost::ref(failed)));
  t.join();

  ASSERT_FALSE(failed.load());
}

// TODO: once we have a low-level dynamic library wrapper, use it and allow testing on non-unix platforms
#if __unix__ && !APPLE
TEST(MallocWrappers, sharedObjectDynamicallyOpened)
{
  void* handle = dlopen("libtest_malloc_wrappers_so.so", RTLD_LAZY|RTLD_GLOBAL);
  ASSERT_TRUE(handle);
  void*(*alloc_func)(size_t) = (void*(*)(size_t))dlsym(handle, "myTestMalloc");
  ASSERT_TRUE(handle);
  void(*free_func)(void*) = (void(*)(void*))dlsym(handle, "myTestFree");
  ASSERT_TRUE(free_func);

  resetThreadAllocInfo();
  void* mem = alloc_func(500);
  free_func(mem);

  AllocInfo info = getThreadAllocInfo();
  ASSERT_EQ(info.mallocs, 1U);
  ASSERT_EQ(info.frees, 1U);

  dlclose(handle);
}
#endif

void doBreakOnMalloc()
{
  setThreadBreakOnAllocOrFree(true);
  void* mem = malloc(500);
  mem = 0;
  setThreadBreakOnAllocOrFree(false);
}

TEST(MallocWrappersDeathTest, breakOnAllocFree)
{
  resetThreadAllocInfo();

  // TODO: Re-enable once ROS 1.1 goes out with the updated version of gtest
  //ASSERT_DEATH_IF_SUPPORTED(doBreakOnMalloc(), "Issuing break due to break_on_alloc_or_free being set");
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  return RUN_ALL_TESTS();
}

