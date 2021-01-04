// yabridge: a Wine VST bridge
// Copyright (C) 2020-2021 Robbert van der Helm
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <pluginterfaces/vst/ivstaudioprocessor.h>

#include "../../common.h"
#include "../base.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

/**
 * Wraps around `IAudioPresentationLatency` for serialization purposes. This is
 * instantiated as part of `Vst3PluginProxy`.
 */
class YaAudioPresentationLatency
    : public Steinberg::Vst::IAudioPresentationLatency {
   public:
    /**
     * These are the arguments for creating a `YaAudioPresentationLatency`.
     */
    struct ConstructArgs {
        ConstructArgs();

        /**
         * Check whether an existing implementation implements
         * `IAudioPresentationLatency` and read arguments from it.
         */
        ConstructArgs(Steinberg::IPtr<Steinberg::FUnknown> object);

        /**
         * Whether the object supported this interface.
         */
        bool supported;

        template <typename S>
        void serialize(S& s) {
            s.value1b(supported);
        }
    };

    /**
     * Instantiate this instance with arguments read from another interface
     * implementation.
     */
    YaAudioPresentationLatency(const ConstructArgs&& args);

    inline bool supported() const { return arguments.supported; }

    /**
     * Message to pass through a call to
     * `IAudioPresentationLatency::setAudioPresentationLatencySamples(dir,
     * bus_index, latency_in_samples` to the Wine plugin host.
     */
    struct SetAudioPresentationLatencySamples {
        using Response = UniversalTResult;

        native_size_t instance_id;

        Steinberg::Vst::BusDirection dir;
        int32 bus_index;
        uint32 latency_in_samples;

        template <typename S>
        void serialize(S& s) {
            s.value8b(instance_id);
            s.value4b(dir);
            s.value4b(bus_index);
            s.value4b(latency_in_samples);
        }
    };

    virtual tresult PLUGIN_API
    setAudioPresentationLatencySamples(Steinberg::Vst::BusDirection dir,
                                       int32 busIndex,
                                       uint32 latencyInSamples) override = 0;

   protected:
    ConstructArgs arguments;
};

#pragma GCC diagnostic pop
