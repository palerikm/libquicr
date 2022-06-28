#pragma once

#include <cassert>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <atomic>

#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>

#include <map>

// picoquic/quicr dependencies
#include "picoquic.h"
#include "picoquic_config.h"
#include "picoquic_utils.h"
#include "quicrq_reassembly.h"
#include "quicrq_relay.h"

#include <quicr/quicr_client.h>

namespace quicr::internal {

class QuicRTransport;

// Context shared with th the underlying quicr stack
struct TransportContext
{
  quicrq_ctx_t* qr_ctx;
  quicrq_cnx_ctx_t* cn_ctx;
  QuicRTransport* transport;
};

struct PublisherContext
{
  std::string quicr_name;
  quicrq_media_object_source_ctx_t* object_source_ctx; // used with/object api
  QuicRTransport* transport;
};

struct ConsumerContext
{
  std::string quicr_name;
  quicrq_object_stream_consumer_ctx* object_consumer_ctx;
  QuicRTransport* transport;
};

///
/// QuicrTransport - Manages QuicR Protocol
///
class QuicRTransport
{
public:
  // Payload and metadata
  struct Data
  {
    std::string quicr_name;
    bytes app_data;
  };

  QuicRTransport(QuicRClient::Delegate& delegate_in,
                 const std::string& sfuName_in,
                 const uint16_t sfuPort_in);

  ~QuicRTransport();

  void register_publish_sources(const std::vector<std::string>& publishers);
  void unregister_publish_sources(const std::vector<std::string>& publishers);
  void publish_named_data(const std::string& url, Data&& data);
  void subscribe(const std::vector<std::string>& names);
  void unsubscribe(const std::vector<std::string>& names);
  void start();
  bool ready();
  void close();

  // callback registered with the underlying quicr stack
  static int quicr_callback(picoquic_cnx_t* cnx,
                            uint64_t stream_id,
                            uint8_t* bytes,
                            size_t length,
                            picoquic_call_back_event_t fin_or_event,
                            void* callback_ctx,
                            void* v_stream_ctx);

  // Reports if the underlying quic stack is ready
  // for application messages
  std::mutex quicConnectionReadyMutex;
  bool quicConnectionReady;

  // Thread and its function managing quic context.
  std::thread quicTransportThread;
  int runQuicProcess();
  static int quicTransportThreadFunc(QuicRTransport* netTransportQuic)
  {
    if (!netTransportQuic) {
      throw std::runtime_error("Transpor not initialized");
    }
    return netTransportQuic->runQuicProcess();
  }

  // APIs interacting with underlying quic stack for sending and receiving the
  // data
  bool hasDataToSendToNet();
  bool getDataToSendToNet(Data& data_out);
  void recvDataFromNet(Data& data_in);
  void on_media_close(ConsumerContext* cons_ctx);
  const PublisherContext& get_publisher_context(const std::string& name) const
  {
    return publishers.at(name);
  }

  void wake_up_all_sources();

  std::atomic<bool> shutting_down = false;
  std::atomic<bool> closed = false;
private:
  // Queue of data to be published
  std::queue<Data> sendQ;
  std::mutex sendQMutex;

  // todo : implement RAII
  picoquic_quic_config_t config;
  quicrq_ctx_t* quicr_ctx = nullptr;
  quicrq_cnx_ctx_t* cnx_ctx = nullptr;
  picoquic_quic_t* quic = nullptr;

  // Underlying context for the transport
  TransportContext transport_context;
  // source -> pub_ctx
  std::map<std::string, PublisherContext> publishers = {};
  // source_ -> consumer_ctx
  std::map<std::string, ConsumerContext> consumers = {};
  // Handler of transport events from the application
  QuicRClient::Delegate& application_delegate;

  LogHandler& logger;
};

} // namespace quicr