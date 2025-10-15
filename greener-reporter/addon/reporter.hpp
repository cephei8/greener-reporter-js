#ifndef GREENER_REPORTER_ADDON_REPORTER_HPP
#define GREENER_REPORTER_ADDON_REPORTER_HPP

#include <node_api.h>

struct greener_reporter;

class reporter {
public:
  static napi_value register_class(napi_env env, napi_value exports);
  static void finalizer(napi_env env, void *obj, void *finalize_hint);

private:
  ~reporter();

  static napi_value ctor(napi_env env, napi_callback_info info);
  static napi_value create_session(napi_env env, napi_callback_info info);
  static napi_value create_testcase(napi_env env, napi_callback_info info);
  static napi_value shutdown(napi_env env, napi_callback_info info);

  greener_reporter *reporter_ = nullptr;
  napi_env env_ = nullptr;
  napi_ref wrapper_ = nullptr;
};

#endif // GREENER_REPORTER_ADDON_REPORTER_HPP
