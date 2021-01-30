
#ifndef SERIALIZER_BASE_H_
#define SERIALIZER_BASE_H_

#include "Definitions.h"

class Payload;

class Serializer
{
  private:
    // Queue of deserialized payloads
    PayloadQueue _payloads;

    // Queue of serialized payloads
    BufferQueue _buffers;

    // Queue of errors that occured
    ErrorQueue _errors;


  protected:
    // Push a completed payload to the private buffer
    void pushPayload( Payload* );

    // Push a full character buffer (signal payload) to the internal queue
    void pushBuffer( Buffer* );

    // Push a char* pointer to the error queue
    void pushError( const char* );


  public:
    Serializer() {}
    virtual ~Serializer() {}

    // Turns a payload into a character buffer for writing
    virtual void serialize( const Payload* ) = 0;

    // Turn a character buffer into payload
    virtual void deserialize( const Buffer* ) = 0;


    // Writes the internal buffer to the output buffer
    Buffer* getBuffer();

    // Return a flag to indicate there are write buffers ready to send
    bool bufferEmpty() const;


    // Return a finished message
    Payload* getPayload();

    // Return a flag to state that a message is finished. Is called after each character is pushed.
    bool payloadEmpty() const;


    // Return an error string describing the error
    const char* getError();

    // Declares an error has happened
    bool errorEmpty() const;

};

#endif // SERIALIZER_BASE_H_

