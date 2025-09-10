#include "sink_stream_handler.h"

namespace bluetooth_classic_multiplatform {

void SinkStreamHandler::cancel() {
    if (sink.get() != nullptr && streamActive) {
        streamActive = false;
        sink->EndOfStream();
    }
}

std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
SinkStreamHandler::OnListenInternal(
    const flutter::EncodableValue* arguments,
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>&& events) {
    sink = std::move(events);
    streamActive = true;
    return nullptr;
};

std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
SinkStreamHandler::OnCancelInternal(const flutter::EncodableValue* arguments) {
    if (sink.get() != nullptr && streamActive) {
        streamActive = false;
        sink->EndOfStream();
        sink.reset();
    }
    return nullptr;
};

}  // namespace bluetooth_classic_multiplatform