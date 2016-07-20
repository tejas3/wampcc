

#include <topic.h>
#include <dealer_service.h>
#include <logger.h>
#include <kernel.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <random>

#include <unistd.h>
#include <string.h>
#include <sys/time.h>

XXX::logger * logger = new XXX::ConsoleLogger(XXX::ConsoleLogger::eStdout,
                                              XXX::logger::eAll,
                                              true);

struct callback_t
{
  callback_t(XXX::kernel* s, const char* d)
    : svc(s),
      request(d)
  {
  }
  XXX::kernel* svc;
  const char* request;
};



XXX::basic_text_model text_data("initial");
XXX::topic model_publisher("topic_1", &text_data);

XXX::basic_list_model basic_list;
XXX::topic list_publisher("planets", &basic_list);

void procedure_error_cb(XXX::invoke_details& invocation)
{
  const callback_t* cbdata = (callback_t*) invocation.user;

  /* called when a procedure within a CALLEE is triggered */
  auto __logptr = logger;
  _INFO_ ("CALLEE has procuedure '"<< invocation.uri << "' invoked, args: " << invocation.args.args_list
          << ", user:" << cbdata->request );

  throw XXX::wamp_error("user.error.rpc_failed");

}

void procedure_cb(XXX::invoke_details& invocation)
{
  const callback_t* cbdata = (callback_t*) invocation.user;

  /* called when a procedure within a CALLEE is triggered */
  auto __logptr = logger;
  _INFO_ ("CALLEE has procuedure '"<< invocation.uri << "' invoked, args: " << invocation.args.args_list
          << ", user:" << cbdata->request );

//  throw std::runtime_error("bad alloc");
  auto my_args = invocation.args;

  my_args.args_list = jalson::json_array();
  jalson::json_array & arr = my_args.args_list.as_array();
  arr.push_back("hello");
  arr.push_back("back");

  invocation.yield_fn(my_args);
}


std::string get_timestamp()
{

  // get current time
  timeval now;
  struct timezone * const tz = NULL; /* not used on Linux */
  gettimeofday(&now, tz);

  struct tm _tm;
  localtime_r(&now.tv_sec, &_tm);

  std::ostringstream os;
  os << _tm.tm_hour << ":" << _tm.tm_min << ":" << _tm.tm_sec;

  return os.str();
}

XXX::dealer_service * g_dealer = nullptr;

void publisher_tep()
{
  const char* const names[] = { "sun", "mercury", "venus", "earth", "mars", "jupiter", "saturn", "uranus", "neptune", "pluto"};

  std::random_device rd;
  int seed = 0;
  std::mt19937 gen( seed );
  std::uniform_int_distribution<> dis(0, 9);
  while(true)
  {
    usleep(1000000 * 3);
    std::string newvalue = "0000____" + get_timestamp();


    XXX::wamp_args wargs;
    wargs.args_list = jalson::json_value::make_array();
    wargs.args_list.as_array().push_back( newvalue );

    // if (g_dealer) g_dealer->publish("USERHB",
    //                                 "default_realm",
    //                                 jalson::json_object(),
    //                                 wargs);

    //text_data.set_value(newvalue);


    switch ( dis(gen) )
    {
      case 0 : basic_list.insert(0, names[dis(gen)]); break;
      case 1 : if (basic_list.copy_value().size()>0 && basic_list.copy_value().size()<10) basic_list.insert(basic_list.copy_value().size()-1, names[dis(gen)]); break;
      case 2 : if (basic_list.copy_value().size()>0) basic_list.replace(0, names[dis(gen)]); break;
      case 3 : if (basic_list.copy_value().size()>0) basic_list.replace(basic_list.copy_value().size()-1, names[dis(gen)]); break;
      case 4 : if (basic_list.copy_value().size()>0) basic_list.erase(0); break;
      case 5 : if (basic_list.copy_value().size()>0) basic_list.erase(basic_list.copy_value().size()-1); break;
      default: if (basic_list.copy_value().size()<10) basic_list.push_back( names[dis(gen)] );
    };



  }

}

int main(int /* argc */, char** /* argv */)
{
  auto logfn = XXX::nlogger::stdlog(std::cout,
                                    XXX::nlogger::levels_all(),
                                    true);

  std::unique_ptr<XXX::kernel> mycs ( new XXX::kernel(logger, logfn) );
  mycs->start();

  XXX::dealer_service * dealer = new XXX::dealer_service(*(mycs.get()), nullptr);
  g_dealer = dealer;

  model_publisher.add_target("default_realm", g_dealer);
  list_publisher.add_target("default_realm", g_dealer);

  XXX::auth_provider server_auth;
  server_auth.permit_user_realm = [](const std::string& /*user*/, const std::string& /*realm*/){ return true; };
  server_auth.get_user_secret   = [](const std::string& /*user*/, const std::string& /*realm*/){ return "secret2"; };

  // start listening for sessions
  std::future<int> fut_listen_err = dealer->listen(55555, server_auth);
  std::future_status status = fut_listen_err.wait_for(std::chrono::seconds(2));

  if (status == std::future_status::ready)
  {
    int err = fut_listen_err.get();
    if (err)
    {
      std::cout << "listen failed, error " << err << ", " << strerror(err) <<  "\n";
      return err;
    }
  }
  else
  {
    std::cout << "timeout waiting for listen socket\n";
    return 1;
  }


  // mycs->add_topic( &topic );


  std::thread publisher( publisher_tep );

  //XXX::dealer_service* dealer = mycs->get_dealer();

  std::unique_ptr<callback_t> cb1( new callback_t(mycs.get(),"my_run") );
  std::unique_ptr<callback_t> cb2( new callback_t(mycs.get(),"my_error") );

  dealer->register_procedure("default_realm",
                             "run",
                             jalson::json_object(),
                             procedure_cb, (void*) cb1.get());

  dealer->register_procedure("default_realm",
                             "erun",
                             jalson::json_object(),
                             procedure_error_cb, (void*) cb2.get());

  while(1) sleep(10);

  // explicit deletion for better control

  publisher.join();
  mycs.reset();
  delete logger;
  return 0;
}
