// Copyright (c) 2020 by Robert Bosch GmbH, Apex.AI. All rights reserved.
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

#include "iceoryx_posh/popo/typed_subscriber.hpp"
#include "iceoryx_posh/popo/user_trigger.hpp"
#include "iceoryx_posh/popo/wait_set.hpp"
#include "iceoryx_posh/runtime/posh_runtime.hpp"
#include "topic_data.hpp"

#include <csignal>
#include <iostream>

iox::popo::UserTrigger shutdownTrigger;

static void sigHandler(int f_sig [[gnu::unused]])
{
    // caught SIGINT, now exit gracefully
    shutdownTrigger.trigger(); // unblock waitsets
}

int main()
{
    // register sigHandler for SIGINT
    signal(SIGINT, sigHandler);

    // initialize runtime
    iox::runtime::PoshRuntime::initRuntime("iox-ex-subscriber-typed");

    // initialized subscribers
    iox::popo::SubscriberOptions subscriberOptions;
    subscriberOptions.queueCapacity = 10U;
    iox::popo::TypedSubscriber<Position> typedSubscriber({"Odometry", "Position", "Vehicle"}, subscriberOptions);
    typedSubscriber.subscribe();

    // set up waitset
    iox::popo::WaitSet<> waitSet{};
    waitSet.attachEvent(typedSubscriber, iox::popo::SubscriberEvent::HAS_SAMPLES);
    waitSet.attachEvent(shutdownTrigger);

    // run until interrupted by Ctrl-C
    while (true)
    {
        auto eventVector = waitSet.wait();
        for (auto& event : eventVector)
        {
            if (event->doesOriginateFrom(&shutdownTrigger))
            {
                return (EXIT_SUCCESS);
            }
            else
            {
                auto subscriber = event->getOrigin<iox::popo::TypedSubscriber<Position>>();
                subscriber->take()
                    .and_then([](iox::popo::Sample<const Position>& position) {
                        std::cout << "Got value: (" << position->x << ", " << position->y << ", " << position->z << ")"
                                  << std::endl;
                    })
                    .if_empty([] { std::cout << "Didn't get a value, but do something anyway." << std::endl; })
                    .or_else([](iox::popo::ChunkReceiveError) { std::cout << "Error receiving chunk." << std::endl; });
            }
        }
    }

    return (EXIT_SUCCESS);
}
