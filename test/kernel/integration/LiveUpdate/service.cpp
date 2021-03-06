// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <service>
#include <net/inet4>
#include <cstdio>
#include "liu.hpp"

using storage_func_t = liu::LiveUpdate::storage_func;
extern storage_func_t begin_test_boot();

void Service::start()
{
  auto func = begin_test_boot();

  if (liu::LiveUpdate::is_resumable() == false)
  {
    auto& inet = net::Super_stack::get<net::IP4>(0);
    setup_liveupdate_server(inet, 666, func);
    // signal test.py that the server is up
    printf("Ready to receive binary blob\n");
  }
}
