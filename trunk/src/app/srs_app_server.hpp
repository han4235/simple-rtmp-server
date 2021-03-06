/*
The MIT License (MIT)

Copyright (c) 2013-2015 winlin

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef SRS_APP_SERVER_HPP
#define SRS_APP_SERVER_HPP

/*
#include <srs_app_server.hpp>
*/

#include <srs_core.hpp>

#include <vector>
#include <string>

#include <srs_app_st.hpp>
#include <srs_app_reload.hpp>
#include <srs_app_source.hpp>
#include <srs_app_hls.hpp>
#include <srs_app_listener.hpp>

class SrsServer;
class SrsConnection;
class SrsHttpServeMux;
class SrsHttpServer;
class SrsIngester;
class SrsHttpHeartbeat;
class SrsKbps;
class SrsConfDirective;
class ISrsTcpHandler;
class ISrsUdpHandler;
class SrsUdpListener;
class SrsTcpListener;

// listener type for server to identify the connection,
// that is, use different type to process the connection.
enum SrsListenerType 
{
    // RTMP client,
    SrsListenerRtmpStream       = 0,
    // HTTP api,
    SrsListenerHttpApi          = 1,
    // HTTP stream, HDS/HLS/DASH
    SrsListenerHttpStream       = 2,
    // UDP stream, MPEG-TS over udp.
    SrsListenerMpegTsOverUdp    = 3,
    // TCP stream, RTSP stream.
    SrsListenerRtsp             = 4,
};

/**
* the common tcp listener, for RTMP/HTTP server.
*/
class SrsListener
{
protected:
    SrsListenerType _type;
protected:
    std::string _ip;
    int _port;
    SrsServer* _server;
public:
    SrsListener(SrsServer* server, SrsListenerType type);
    virtual ~SrsListener();
public:
    virtual SrsListenerType type();
    virtual int listen(std::string ip, int port) = 0;
};

/**
* tcp listener.
*/
class SrsStreamListener : virtual public SrsListener, virtual public ISrsTcpHandler
{
private:
    SrsTcpListener* listener;
public:
    SrsStreamListener(SrsServer* server, SrsListenerType type);
    virtual ~SrsStreamListener();
public:
    virtual int listen(std::string ip, int port);
// ISrsTcpHandler
public:
    virtual int on_tcp_client(st_netfd_t stfd);
};

#ifdef SRS_AUTO_STREAM_CASTER
/**
* the tcp listener, for rtsp server.
*/
class SrsRtspListener : virtual public SrsListener, virtual public ISrsTcpHandler
{
private:
    SrsTcpListener* listener;
    ISrsTcpHandler* caster;
public:
    SrsRtspListener(SrsServer* server, SrsListenerType type, SrsConfDirective* c);
    virtual ~SrsRtspListener();
public:
    virtual int listen(std::string ip, int port);
// ISrsTcpHandler
public:
    virtual int on_tcp_client(st_netfd_t stfd);
};

/**
* the udp listener, for udp server.
*/
class SrsUdpCasterListener : public SrsListener
{
private:
    SrsUdpListener* listener;
    ISrsUdpHandler* caster;
public:
    SrsUdpCasterListener(SrsServer* server, SrsListenerType type, SrsConfDirective* c);
    virtual ~SrsUdpCasterListener();
public:
    virtual int listen(std::string ip, int port);
};
#endif

/**
* convert signal to io,
* @see: st-1.9/docs/notes.html
*/
class SrsSignalManager : public ISrsThreadHandler
{
private:
    /* Per-process pipe which is used as a signal queue. */
    /* Up to PIPE_BUF/sizeof(int) signals can be queued up. */
    int sig_pipe[2];
    st_netfd_t signal_read_stfd;
private:
    SrsServer* _server;
    SrsThread* pthread;
public:
    SrsSignalManager(SrsServer* server);
    virtual ~SrsSignalManager();
public:
    virtual int initialize();
    virtual int start();
// interface ISrsThreadHandler.
public:
    virtual int cycle();
private:
    // global singleton instance
    static SrsSignalManager* instance;
    /* Signal catching function. */
    /* Converts signal event to I/O event. */
    static void sig_catcher(int signo);
};

/**
* the handler to the handle cycle in SRS RTMP server.
*/
class ISrsServerCycle
{
public:
    ISrsServerCycle();
    virtual ~ISrsServerCycle();
public: 
    /**
    * initialize the cycle handler.
    */
    virtual int initialize() = 0;
    /**
    * do on_cycle while server doing cycle.
    */
    virtual int on_cycle(int connections) = 0;
};

/**
* SRS RTMP server, initialize and listen, 
* start connection service thread, destroy client.
*/
class SrsServer : virtual public ISrsReloadHandler
    , virtual public ISrsSourceHandler, virtual public ISrsHlsHandler
{
private:
#ifdef SRS_AUTO_HTTP_API
    SrsHttpServeMux* http_api_mux;
#endif
#ifdef SRS_AUTO_HTTP_SERVER
    SrsHttpServer* http_stream_mux;
#endif
#ifdef SRS_AUTO_HTTP_PARSER
    SrsHttpHeartbeat* http_heartbeat;
#endif
#ifdef SRS_AUTO_INGEST
    SrsIngester* ingester;
#endif
private:
    /**
    * the pid file fd, lock the file write when server is running.
    * @remark the init.d script should cleanup the pid file, when stop service,
    *       for the server never delete the file; when system startup, the pid in pid file
    *       maybe valid but the process is not SRS, the init.d script will never start server.
    */
    int pid_fd;
    /**
    * all connections, connection manager
    */
    std::vector<SrsConnection*> conns;
    /**
    * all listners, listener manager.
    */
    std::vector<SrsListener*> listeners;
    /**
    * signal manager which convert gignal to io message.
    */
    SrsSignalManager* signal_manager;
    /**
    * handle in server cycle.
    */
    ISrsServerCycle* handler;
    /**
    * user send the signal, convert to variable.
    */
    bool signal_reload;
    bool signal_gmc_stop;
public:
    SrsServer();
    virtual ~SrsServer();
public:
    /**
    * the destroy is for gmc to analysis the memory leak,
    * if not destroy global/static data, the gmc will warning memory leak.
    * in service, server never destroy, directly exit when restart.
    */
    virtual void destroy();
// server startup workflow, @see run_master()
public:
    virtual int initialize(ISrsServerCycle* cycle_handler);
    virtual int initialize_signal();
    virtual int acquire_pid_file();
    virtual int initialize_st();
    virtual int listen();
    virtual int register_signal();
    virtual int http_handle();
    virtual int ingest();
    virtual int cycle();
// server utility
public:
    /**
    * callback for connection to remove itself.
    * when connection thread cycle terminated, callback this to delete connection.
    * @see SrsConnection.on_thread_stop().
    */
    virtual void remove(SrsConnection* conn);
    /**
    * callback for signal manager got a signal.
    * the signal manager convert signal to io message,
    * whatever, we will got the signo like the orignal signal(int signo) handler.
    * @remark, direclty exit for SIGTERM.
    * @remark, do reload for SIGNAL_RELOAD.
    * @remark, for SIGINT and SIGUSR2:
    *       no gmc, directly exit.
    *       for gmc, set the variable signal_gmc_stop, the cycle will return and cleanup for gmc.
    */
    virtual void on_signal(int signo);
private:
    /**
    * the server thread main cycle,
    * update the global static data, for instance, the current time,
    * the cpu/mem/network statistic.
    */
    virtual int do_cycle();
    /**
    * listen at specified protocol.
    */
    virtual int listen_rtmp();
    virtual int listen_http_api();
    virtual int listen_http_stream();
    virtual int listen_stream_caster();
    /**
    * close the listeners for specified type, 
    * remove the listen object from manager.
    */
    virtual void close_listeners(SrsListenerType type);
    /**
    * resample the server kbs.
    */
    virtual void resample_kbps();
// internal only
public:
    /**
    * when listener got a fd, notice server to accept it.
    * @param type, the client type, used to create concrete connection, 
    *       for instance RTMP connection to serve client.
    * @param client_stfd, the client fd in st boxed, the underlayer fd.
    */
    virtual int accept_client(SrsListenerType type, st_netfd_t client_stfd);
// interface ISrsThreadHandler.
public:
    virtual int on_reload_listen();
    virtual int on_reload_pid();
    virtual int on_reload_vhost_added(std::string vhost);
    virtual int on_reload_vhost_removed(std::string vhost);
    virtual int on_reload_http_api_enabled();
    virtual int on_reload_http_api_disabled();
    virtual int on_reload_http_stream_enabled();
    virtual int on_reload_http_stream_disabled();
    virtual int on_reload_http_stream_updated();
// interface ISrsSourceHandler
public:
    virtual int on_publish(SrsSource* s, SrsRequest* r);
    virtual void on_unpublish(SrsSource* s, SrsRequest* r);
// interface ISrsHlsHandler
public:
    virtual int on_hls_publish(SrsRequest* r);
    virtual int on_update_m3u8(SrsRequest* r, std::string m3u8);
    virtual int on_update_ts(SrsRequest* r, std::string uri, std::string ts);
    virtual int on_hls_unpublish(SrsRequest* r);
};

#endif

