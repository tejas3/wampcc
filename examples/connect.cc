#include "uv.h"

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>

#include <unistd.h>

static uv_tcp_t tcp_handle_1;
static uv_tcp_t tcp_handle_2;
static uv_async_t async;

void alloc_buffer(uv_handle_t* /* handle */,
                  size_t suggested_size,
                  uv_buf_t* buf )
{
  *buf = uv_buf_init((char *) malloc(suggested_size), suggested_size);
}

/* Main reader callback */
void on_read(uv_stream_t*  uvh,
             ssize_t nread ,
             const uv_buf_t* buf)
{
  std::cout << __FUNCTION__ << "\n";
}
static void connect_cb(uv_connect_t* req, int status)
{
  std::cout << __FUNCTION__ << "\n";
  uv_read_start(req->handle,
                alloc_buffer,
                on_read);
}


std::mutex queue_mutex;
std::vector< void * > async_work_queue;

int worker_thread()
{
  sleep(5); // sleep so that IO thread should be running
  std::cout << __FUNCTION__ << "\n";


  {
    std::unique_lock<std::mutex> guard( queue_mutex );
    async_work_queue.push_back( new char[1] );

    // TODO: think about this; does the signal need to be inside the mutex?

    // Wakeup the event loop and call the async handle’s callback. This call is
    // non-blocking, ie, returns immediately.
    std::cout << "pushing work\n";
    uv_async_send( &async );

  }


  while (1){ sleep(100); }


}

/*
 */
void async_cb(uv_async_t* handle)
{
  std::cout << __FUNCTION__ << "\n";

  {
    std::unique_lock<std::mutex> guard( queue_mutex );
    async_work_queue.clear();
  }
  uv_connect_t connect_req;
  struct sockaddr_in addr;

  uv_ip4_addr("127.0.0.1", 55555, &addr);

  // Initialize the handle. No socket is created as of yet.
  uv_tcp_init(uv_default_loop(), &tcp_handle_2);

  // Attempt to establish the connection. The callback is made when the
  // connection has been established or when a connection error happened
  uv_tcp_connect(&connect_req,
                 &tcp_handle_2,
                 (const struct sockaddr*) &addr,
                 connect_cb);

}

static void io_on_timer(uv_timer_t* handle)
{
  std::cout << __FUNCTION__ << "\n";
}

int main(int argc, char** arvc)
{

  std::thread thread2(worker_thread);

  uv_connect_t connect_req;
  struct sockaddr_in addr;

  uv_async_init(uv_default_loop(), &async, async_cb);

  uv_timer_t m_timer;
  uv_timer_init(uv_default_loop(), &m_timer);
  uv_timer_start(&m_timer, io_on_timer, 60000,60000);

  while(1)
  {
    std::cout << "entering IO loop\n";
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  }

  return 0;
}