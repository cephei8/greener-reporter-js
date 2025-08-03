#ifndef GREENER_SERVERMOCK_ADDON_SERVERMOCK_HPP
#define GREENER_SERVERMOCK_ADDON_SERVERMOCK_HPP

#include <node_api.h>

struct greener_servermock;

class servermock {
public:
  static napi_value register_class(napi_env env, napi_value exports);
  static void finalizer(napi_env env, void *obj, void *finalize_hint);

private:
  ~servermock();

  static napi_value ctor(napi_env env, napi_callback_info info);
  static napi_value serve(napi_env env, napi_callback_info info);
  static napi_value get_port(napi_env env, napi_callback_info info);
  static napi_value assert_calls(napi_env env, napi_callback_info info);
  static napi_value fixture_names(napi_env env, napi_callback_info info);
  static napi_value fixture_calls(napi_env env, napi_callback_info info);
  static napi_value fixture_responses(napi_env env, napi_callback_info info);
  static napi_value shutdown(napi_env env, napi_callback_info info);

  greener_servermock *servermock_ = nullptr;
  napi_env env_ = nullptr;
  napi_ref wrapper_ = nullptr;
};

#endif // GREENER_SERVERMOCK_ADDON_SERVERMOCK_HPP
