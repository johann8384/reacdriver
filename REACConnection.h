/*
 *  REACConnection.h
 *  REAC
 *
 *  Created by Per Eckerdal on 02/01/2011.
 *  Copyright 2011 Per Eckerdal. All rights reserved.
 *
 */

#ifndef _REACCONNECTION_H
#define _REACCONNECTION_H

#include <IOKit/audio/IOAudioDevice.h>
#include <IOKit/IOCommandGate.h>
#include <net/kpi_interface.h>
#include <sys/kpi_mbuf.h>
#include <net/kpi_interfacefilter.h>

#include "REACDataStream.h"

#define REAC_MAX_CHANNEL_COUNT 40
#define REAC_PACKETS_PER_SECOND 8000
#define REAC_RESOLUTION 3 // 3 bytes per sample per channel
#define REAC_SAMPLES_PER_PACKET 12
#define ETHER_ADDR_LEN 6

#define REAC_SAMPLE_RATE REAC_PACKETS_PER_SECOND * REAC_SAMPLES_PER_PACKET

#define REAC_ENDING 0xeac2 // TODO Endianness?
#define REAC_ENDING_LENGTH 4


#define EthernetHeader				com_pereckerdal_driver_EthernetHeader
#define REACDeviceInfo              com_pereckerdal_driver_REACDeviceInfo
#define REACConnection              com_pereckerdal_driver_REACConnection


/* Ethernet header */
struct EthernetHeader {
	UInt8 dhost[ETHER_ADDR_LEN]; /* Destination host address */
	UInt8 shost[ETHER_ADDR_LEN]; /* Source host address */
	UInt16 type; /* IP? ARP? RARP? etc */
};

struct REACDeviceInfo {
    UInt8 addr[ETHER_ADDR_LEN];
    UInt32 in_channels;
    UInt32 out_channels;
};

class REACConnection;

// Device is NULL on disconnect
typedef void(*reac_connection_callback_t)(REACConnection *proto, void **cookieA, void **cookieB, REACDeviceInfo *device);
// Is only called when the connection callback has indicated that there is a connection
typedef void(*reac_samples_callback_t)(REACConnection *proto, void **cookieA, void **cookieB, UInt8 **data, UInt32 *bufferSize);
// Is only called when in REAC_MASTER or REAC_SLAVE mode and the connection callback has
// indicated that there is a connection.
typedef void(*reac_get_samples_callback_t)(REACConnection *proto, void **cookieA, void **cookieB, UInt8 **data, UInt32 *bufferSize);


// This class is not thread safe; the only functions that can be called
// without being synchronized to the work loop are the interface filter
// callbacks. The samplesCallback and connectionCallback callbacks are
// guaranteed to be called from within the work loop.
//
// TODO Private constructor/assignment operator/destructor?
class REACConnection : public OSObject {
    OSDeclareDefaultStructors(REACConnection)
public:
    enum REACMode {
        REAC_MASTER, REAC_SLAVE, REAC_SPLIT
    };
    
    virtual bool initWithInterface(IOWorkLoop *workLoop, ifnet_t interface, REACMode mode,
                                   reac_connection_callback_t connectionCallback,
                                   reac_samples_callback_t samplesCallback,
                                   reac_get_samples_callback_t getSamplesCallback,
                                   void *cookieA,
                                   void *cookieB);
    static REACConnection *withInterface(IOWorkLoop *workLoop, ifnet_t interface, REACMode mode,
                                         reac_connection_callback_t connectionCallback,
                                         reac_samples_callback_t samplesCallback,
                                         reac_get_samples_callback_t getSamplesCallback,
                                         void *cookieA,
                                         void *cookieB);
protected:
    // Object destruction method that is used by free, and initWithInterface on failure.
    virtual void deinit();
    virtual void free();
public:
    bool listen();
    void detach();
    
    const REACDeviceInfo *getDeviceInfo() const;
    bool isListening() const { return listening; }
    bool isConnected() const { return connected; }
    // If you want to continue using the ifnet_t object, make sure to call
    // ifnet_reference on it, as REACConnection will release it when it is freed.
    ifnet_t getInterface() const { return interface; }
    REACMode getMode() const { return mode; }

protected:
    // IOKit handles
    IOWorkLoop         *workLoop;
    IOTimerEventSource *timerEventSource;
    IOCommandGate      *filterCommandGate;
    UInt64              timeoutNS;
    UInt64              nextTime;                // the estimated time the timer will fire next
    
    // Network handles
    ifnet_t             interface;
    interface_filter_t  filterRef;
    
    // Callback variables
    reac_connection_callback_t connectionCallback;
    reac_samples_callback_t samplesCallback;
    reac_get_samples_callback_t getSamplesCallback;
    void *cookieA;
    void *cookieB;
    
    // Variables for keeping track of when a connection is lost
    UInt64              lastSeenConnectionCounter;
    UInt64              connectionCounter;
    
    // Connection state variables
    REACMode            mode;
    bool                listening;
    bool                connected;
    REACDataStream     *dataStream;
    REACDeviceInfo     *deviceInfo;
    UInt16              lastCounter; // Tracks input REAC counter
    
    static void timerFired(OSObject *target, IOTimerEventSource *sender);
    
    IOReturn getAndPushSamples();
    // When sampleBuffer is NULL, the sample data will be zeros (and bufSize will be disregarded).
    IOReturn pushSamples(UInt32 bufSize, UInt8 *sampleBuffer);
    
    static void filterCommandGateMsg(OSObject *target, void* type, void* protocol, void* deviceInfo, void* audioEnginePointer);
    
    static errno_t filterInputFunc(void *cookie,
                                   ifnet_t interface, 
                                   protocol_family_t protocol,
                                   mbuf_t *data,
                                   char **frame_ptr);        
    static void filterDetachedFunc(void *cookie,
                                   ifnet_t interface);
    
};


#endif
