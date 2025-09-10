#pragma once
#include <flutter/event_channel.h>
#include <flutter/standard_method_codec.h>

#include <memory>

namespace bluetooth_classic_multiplatform {
class SinkStreamHandler
    : public flutter::StreamHandler<flutter::EncodableValue> {
   public:
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> sink;

    void cancel();

   protected:
    bool streamActive = false;
    std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
    OnListenInternal(
        const flutter::EncodableValue* arguments,
        std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events)
        override;

    std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
    OnCancelInternal(const flutter::EncodableValue* arguments) override;
};
}  // namespace bluetooth_classic_multiplatform