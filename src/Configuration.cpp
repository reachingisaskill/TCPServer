
#include "Configuration.h"
#include "Exception.h"


namespace Stewardess
{

  Configuration::Configuration( int p ) :
    _data()
  {
    _data.portNumber = p;
    _data.workerTickTime = { 1, 0 };
    _data.minTickTime = 1;
    _data.tickTimeModifier = 1.0;
    _data.readTimeout = { 3, 0 };
    _data.writeTimeout = { 3, 0 };
    _data.deathTime = { 5, 0 };
    _data.connectionCloseOnShutdown = true;
    _data.bufferSize = 4096;
    _data.numThreads = 2;
    _data.requestListener = false;
    _data.requestSignalHandler = true;
  }


  Configuration::~Configuration()
  {
  }


  void Configuration::setNumberThreads( unsigned n )
  {
    _data.numThreads = n;
  }


  void Configuration::setDefaultBufferSize( size_t buffer_size )
  {
    _data.bufferSize = buffer_size;
  }


  void Configuration::setReadTimeout( unsigned int sec )
  {
    _data.readTimeout.tv_sec = sec;
  }


  void Configuration::setWriteTimeout( unsigned int sec )
  {
    _data.writeTimeout.tv_sec = sec;
  }


  void Configuration::setTickTimeModifier( float m )
  {
    if ( m < 1.0E-6 )
    {
      throw Exception( "Tick time modifier cannot be small or negative." );
    }
    _data.tickTimeModifier = m;
  }


  void Configuration::setMinTickTime( unsigned int m )
  {
    if ( m < 1 )
    {
      throw Exception( "Minimum tick time cannot be less than 1s" );
    }
    _data.minTickTime = m;
  }


  void Configuration::setDeathTime( unsigned int time )
  {
    _data.deathTime.tv_sec = time;
  }


  void Configuration::setCloseConnectionsOnShutdown( bool value )
  {
    _data.connectionCloseOnShutdown = value;
  }


  void Configuration::setRequestListener( bool lis )
  {
    _data.requestListener = lis;
  }


  void Configuration::setRequestSignalHandler( bool sig )
  {
    _data.requestSignalHandler = sig;
  }

}

