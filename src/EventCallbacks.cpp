
#include "EventCallbacks.h"
#include "CallbackInterface.h"
#include "Manager.h"
#include "WorkerThread.h"
#include "Handle.h"
#include "Connection.h"
#include "Serializer.h"

#include <cmath>
#include <cstring>


////////////////////////////////////////////////////////////////////////////////
// Listener call back functions

void listenerAcceptCB( evconnlistener* /*listener*/, evutil_socket_t new_socket, sockaddr* address, int address_length, void* arg )
{
  Manager* data = (Manager*)arg;
  std::cout << "  Listener Accept Called " << std::endl;

  // Choose a worker to handle it
  event_base* worker_base = data->_threads[ data->getNextThread() ]->data.eventBase;

  // Create a buffer event, bound to the tcp socket. When freed it will close the socket.
  bufferevent* buffer_event = bufferevent_socket_new( worker_base, new_socket, BEV_OPT_CLOSE_ON_FREE );

  // Create the connection 
  Connection* connection = new Connection();
  connection->bufferEvent = buffer_event;
  connection->server = &data->_server;
  connection->serializer = data->_server.buildSerializer();
  connection->readBuffer.reserve( data->_configuration.bufferSize );

  // Byte-wise copy the address struct
  std::memcpy( (void*)&connection->socketAddress, (void*)address, address_length );

    
  // Add the new connection to the manager
  data->addConnection( connection );

  // Signal that something has connected
  connection->server->onConnectionEvent( connection->requestHandle(), ConnectionEvent::Connect );


  // Set the call back functions
  bufferevent_setcb( buffer_event, bufferReadCB, bufferWriteCB, bufferEventCB, (void*)connection );

  // Set the time outs for reading & writing only if they're > 0
  bufferevent_set_timeouts( buffer_event, data->getReadTimeout(), data->getWriteTimeout() ); 

  // Enable reading & writing on the buffer event
  bufferevent_enable( buffer_event, EV_READ|EV_WRITE );
}


void listenerErrorCB( evconnlistener* /*listener*/, void* /*data*/ )
{
//  Manager* owner = (Manager*)data;
  std::cout << "  Listener Error Called" << std::endl;

  // Handle listener errors

}


////////////////////////////////////////////////////////////////////////////////
// Server signal callbacks

void interruptSignalCB( evutil_socket_t /*socket*/, short /*what*/, void* arg )
{
  Manager* data = (Manager*)arg;
  std::cerr << "Interrupt received. Time to go." << std::endl;

  // Manager handles the rest.
  data->shutdown();
}


void killTimerCB( evutil_socket_t /*socket*/, short /*what*/, void* arg )
{
  Manager* data = (Manager*)arg;
  std::cout << "Death timer expired. Time to die." << std::endl;

  // Hard stopping of everything
  data->abort();
}


void tickTimerCB( evutil_socket_t /*socket*/, short /*what*/, void* arg )
{
  Manager* data = (Manager*)arg;

  // If the closed connections have no handles, delete them
  data->cleanupClosedConnections();

  // Update the tick time stamp
  TimeStamp new_stamp = std::chrono::system_clock::now();
  auto duration = new_stamp - data->_tickTimeStamp;
  data->_tickTimeStamp = new_stamp;

  // Trigger the callback
  data->_server.onTick( std::chrono::duration_cast<std::chrono::milliseconds>( duration ) );

  // Set the timeout time to the log of the number of connections
  event_add( data->_tickEvent, data->getTickTime() );
}


void workerTickTimerCB( evutil_socket_t /*socket*/, short /*what*/, void* arg )
{
  WorkerData* data = (WorkerData*)arg;
  event_add( data->tickEvent, &data->tickTime );
  // Do nothing. We just need to force libevent to loop around again
}

////////////////////////////////////////////////////////////////////////////////
// Buffer event callback functions

void bufferReadCB( bufferevent* buffer_event, void* arg )
{
  Connection* connection = (Connection*)arg;
  Serializer* serializer = connection->serializer;
  Buffer& buffer = connection->readBuffer;
  std::cout << "  Buffer Read called" << std::endl;

  int result;

  evbuffer* read_data = bufferevent_get_input( buffer_event );

  while( evbuffer_get_length( read_data ) > 0 )
  {
    result = evbuffer_remove( read_data, buffer.data(), buffer.capacity() );

    std::cout << "Read " << result << std::endl;

    if ( result <= 0 )
    {
      buffer.resize( 0 );
      break;
    }

    buffer.resize( result );
    serializer->deserialize( &buffer );

    while( ! serializer->payloadEmpty() )
    {
      std::cout << "Triggering read event." << std::endl;
      connection->server->onRead( connection->requestHandle(), serializer->getPayload() );
    }
  }

  while ( ! serializer->bufferEmpty() )
  {
    Buffer* buf = serializer->getBuffer();
    bufferevent_write( buffer_event, buf->data(), buf->size() );
    delete buf;
  }

  while( ! serializer->errorEmpty() )
  {
    std::cerr << "Serializer error occured: " << serializer->getError() << std::endl;
    connection->server->onConnectionEvent( connection->requestHandle(), ConnectionEvent::Error );
  }

  std::cout << "  Finished reading" << std::endl;
  connection->touchAccess();
}


void bufferWriteCB( bufferevent* buffer_event, void* data )
{
  Connection* connection = (Connection*)data;
  Serializer* serializer = connection->serializer;
  std::cout << "  Buffer Write called" << std::endl;

  connection->server->onWrite( connection->requestHandle() );

  while ( ! serializer->bufferEmpty() )
  {
    Buffer* buf = serializer->getBuffer();
    bufferevent_write( buffer_event, buf->data(), buf->size() );
    delete buf;
  }

  while( ! serializer->errorEmpty() )
  {
    std::cerr << "Serializer error occured: " << serializer->getError() << std::endl;
    connection->server->onConnectionEvent( connection->requestHandle(), ConnectionEvent::Error );
  }

  connection->touchAccess();
}


void bufferEventCB( bufferevent* buffer_event, short flags, void* data )
{
  Connection* connection = (Connection*)data;
  Serializer* serializer = connection->serializer;

  std::cout << "  Buffer Event called" << std::endl;

  if ( flags & BEV_EVENT_CONNECTED )
  {
    std::cerr << "Connection succeeded" << std::endl;
    connection->server->onConnectionEvent( connection->requestHandle(), ConnectionEvent::Connect );
  }

  if ( flags & BEV_EVENT_EOF )
  {
    std::cerr << "Socket closed" << std::endl;
    connection->close();
    connection->server->onConnectionEvent( connection->requestHandle(), ConnectionEvent::Disconnect );
  }

  if ( flags & BEV_EVENT_ERROR )
  {
    std::cerr << "ERROR buffer event: " << EVUTIL_SOCKET_ERROR() << std::endl;
    connection->close();
    connection->server->onConnectionEvent( connection->requestHandle(), ConnectionEvent::Error );
  }

//  if ( flags & BEV_EVENT_READING )
//  {
//  }
//
//  if ( flags & BEV_EVENT_WRITING )
//  {
//  }

  if ( flags & BEV_EVENT_TIMEOUT )
  {
    if ( flags & BEV_EVENT_READING )
    {
      std::cerr << "Read Timeout event occured" << std::endl;
    }
    else if ( flags & BEV_EVENT_WRITING )
    {
      std::cerr << "Write Timeout event occured" << std::endl;
    }
    connection->server->onConnectionEvent( connection->requestHandle(), ConnectionEvent::Timeout );
  }

  while ( ! serializer->bufferEmpty() )
  {
    std::cerr << "Writing to buffer" << std::endl;
    Buffer* buf = serializer->getBuffer();
    bufferevent_write( buffer_event, buf->data(), buf->size() );
    delete buf;
  }

  while( ! serializer->errorEmpty() )
  {
    std::cerr << "Serializer error occured: " << serializer->getError() << std::endl;
    connection->server->onConnectionEvent( connection->requestHandle(), ConnectionEvent::Error );
  }

  connection->touchAccess();
}

