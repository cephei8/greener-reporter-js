#include "servermock.hpp"

#include <greener_servermock/greener_servermock.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
void handle_servermock_error(napi_env env,
                             const greener_servermock_error *err) {
  std::stringstream ss;
  ss << "GreenerServermockError: " << err->message;
  const std::string &msg = ss.str();
  napi_throw_error(env, nullptr, msg.c_str());
}

void handle_napi_error(napi_env env) {
  bool pending;
  napi_is_exception_pending(env, &pending);
  if (!pending) {
    const napi_extended_error_info *error_info(nullptr);
    napi_get_last_error_info(env, &error_info);
    const char *err_message = error_info->error_message;
    if (err_message == nullptr) {
      err_message = "internal error";
    }
    napi_throw_error(env, nullptr, err_message);
  }
}
} // namespace

servermock::~servermock() {
  if (servermock_) {
    const greener_servermock_error *err;
    greener_servermock_delete(servermock_, &err);
  }
  napi_delete_reference(env_, wrapper_);
}

napi_value servermock::register_class(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
      {"serve", nullptr, serve, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"getPort", nullptr, get_port, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"assert", nullptr, assert_calls, nullptr, nullptr, nullptr, napi_default,
       nullptr},
      {"fixtureNames", nullptr, fixture_names, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"fixtureCalls", nullptr, fixture_calls, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"fixtureResponses", nullptr, fixture_responses, nullptr, nullptr,
       nullptr, napi_default, nullptr},
      {"shutdown", nullptr, shutdown, nullptr, nullptr, nullptr, napi_default,
       nullptr},
  };

  napi_value ctor_value;
  if (const auto status =
          napi_define_class(env, "Servermock", NAPI_AUTO_LENGTH, ctor, nullptr,
                            std::size(properties), properties, &ctor_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  auto *ctor_ref = new napi_ref;
  if (const auto status = napi_create_reference(env, ctor_value, 1, ctor_ref);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (const auto status = napi_set_instance_data(
          env, ctor_ref,
          [](napi_env env, void *data, void *) {
            const auto *c = static_cast<napi_ref *>(data);
            const auto status = napi_delete_reference(env, *c);
            delete c;

            if (status != napi_ok) {
              handle_napi_error(env);
            }
          },
          nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (const auto status =
          napi_set_named_property(env, exports, "Servermock", ctor_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  return exports;
}

napi_value servermock::ctor(napi_env env, napi_callback_info info) {
  napi_value target;
  if (const auto status = napi_get_new_target(env, info, &target);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  size_t argc = 0;
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, nullptr, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 0) {
    napi_throw_error(env, nullptr, "no arguments expected");
    return nullptr;
  }

  auto *obj = new servermock();
  obj->env_ = env;
  if (const auto status =
          napi_wrap(env, jsthis, obj, finalizer, nullptr, &obj->wrapper_);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  obj->servermock_ = greener_servermock_new();

  return jsthis;
}

void servermock::finalizer(napi_env env, void *obj, void *) {
  static_cast<servermock *>(obj)->~servermock();
}

napi_value servermock::serve(napi_env env, napi_callback_info info) {
  constexpr size_t argc_num = 1;
  size_t argc = argc_num;
  napi_value args[argc_num];
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 1) {
    napi_throw_error(env, nullptr, "invalid number of arguments");
    return nullptr;
  }

  std::string in_responses;
  {
    char buf[1024 * 1024];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[0], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (value_type != napi_string) {
      napi_throw_error(env, nullptr, "responses must be a string");
      return nullptr;
    }
    if (const auto status = napi_get_value_string_utf8(
            env, args[0], buf, std::size(buf), &buf_len);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    in_responses = std::string(buf, buf_len);
  }

  servermock *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_servermock_error *err;
  greener_servermock_serve(obj->servermock_, in_responses.c_str(), &err);

  if (err != nullptr) {
    handle_servermock_error(env, err);
    return nullptr;
  }

  return nullptr;
}

napi_value servermock::assert_calls(napi_env env, napi_callback_info info) {
  constexpr size_t argc_num = 1;
  size_t argc = argc_num;
  napi_value args[argc_num];
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 1) {
    napi_throw_error(env, nullptr, "invalid number of arguments");
    return nullptr;
  }

  char buf[1024 * 1024];
  size_t buf_len;

  napi_valuetype value_type;
  if (const auto status = napi_typeof(env, args[0], &value_type);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (value_type != napi_string) {
    napi_throw_error(env, nullptr, "calls must be a string");
    return nullptr;
  }
  if (const auto status = napi_get_value_string_utf8(env, args[0], buf,
                                                     std::size(buf), &buf_len);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const std::string calls_str(buf, buf_len);

  servermock *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_servermock_error *err;
  greener_servermock_assert(obj->servermock_, calls_str.c_str(), &err);

  if (err != nullptr) {
    handle_servermock_error(env, err);
    return nullptr;
  }

  return nullptr;
}

napi_value servermock::fixture_names(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, nullptr, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 0) {
    napi_throw_error(env, nullptr, "no arguments expected");
    return nullptr;
  }

  servermock *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_servermock_error *err;
  const char **name_ptrs;
  uint32_t num_names;
  greener_servermock_fixture_names(obj->servermock_, &name_ptrs, &num_names,
                                   &err);

  if (err != nullptr) {
    handle_servermock_error(env, err);
    return nullptr;
  }

  std::vector<std::string> names(name_ptrs, name_ptrs + num_names);

  napi_value out_names_value;
  if (const auto status =
          napi_create_array_with_length(env, num_names, &out_names_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  for (size_t i = 0; i < num_names; ++i) {
    napi_value name_value;
    if (const auto status = napi_create_string_utf8(
            env, names[i].c_str(), names[i].length(), &name_value);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (const auto status =
            napi_set_element(env, out_names_value, i, name_value);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }
  }

  return out_names_value;
}

napi_value servermock::fixture_calls(napi_env env, napi_callback_info info) {
  constexpr size_t argc_num = 1;
  size_t argc = argc_num;
  napi_value args[argc_num];
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 1) {
    napi_throw_error(env, nullptr, "invalid number of arguments");
    return nullptr;
  }

  char buf[1024 * 1024];
  size_t buf_len;

  napi_valuetype value_type;
  if (const auto status = napi_typeof(env, args[0], &value_type);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (value_type != napi_string) {
    napi_throw_error(env, nullptr, "fixture_name must be a string");
    return nullptr;
  }
  if (const auto status = napi_get_value_string_utf8(env, args[0], buf,
                                                     std::size(buf), &buf_len);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const std::string fixture_name(buf, buf_len);

  servermock *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_servermock_error *err;
  const char *calls;
  greener_servermock_fixture_calls(obj->servermock_, fixture_name.c_str(),
                                   &calls, &err);

  if (err != nullptr) {
    handle_servermock_error(env, err);
    return nullptr;
  }

  napi_value calls_value;
  if (const auto status =
          napi_create_string_utf8(env, calls, strlen(calls), &calls_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  return calls_value;
}

napi_value servermock::fixture_responses(napi_env env,
                                         napi_callback_info info) {
  constexpr size_t argc_num = 1;
  size_t argc = argc_num;
  napi_value args[argc_num];
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 1) {
    napi_throw_error(env, nullptr, "invalid number of arguments");
    return nullptr;
  }

  char buf[1024 * 1024];
  size_t buf_len;

  napi_valuetype value_type;
  if (const auto status = napi_typeof(env, args[0], &value_type);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (value_type != napi_string) {
    napi_throw_error(env, nullptr, "fixture_name must be a string");
    return nullptr;
  }
  if (const auto status = napi_get_value_string_utf8(env, args[0], buf,
                                                     std::size(buf), &buf_len);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const std::string fixture_name(buf, buf_len);

  servermock *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_servermock_error *err;
  const char *responses;
  greener_servermock_fixture_responses(obj->servermock_, fixture_name.c_str(),
                                       &responses, &err);

  if (err != nullptr) {
    handle_servermock_error(env, err);
    return nullptr;
  }

  napi_value responses_value;
  if (const auto status = napi_create_string_utf8(
          env, responses, strlen(responses), &responses_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  return responses_value;
}

napi_value servermock::shutdown(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, nullptr, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 0) {
    napi_throw_error(env, nullptr, "no arguments expected");
    return nullptr;
  }

  servermock *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_servermock_error *err;
  greener_servermock_delete(obj->servermock_, &err);
  obj->servermock_ = nullptr;

  if (err != nullptr) {
    handle_servermock_error(env, err);
    return nullptr;
  }

  return nullptr;
}

napi_value servermock::get_port(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, nullptr, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 0) {
    napi_throw_error(env, nullptr, "no arguments expected");
    return nullptr;
  }

  servermock *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_servermock_error *err;
  int port = greener_servermock_get_port(obj->servermock_, &err);

  if (err != nullptr) {
    handle_servermock_error(env, err);
    return nullptr;
  }

  napi_value port_value;
  if (const auto status = napi_create_int32(env, port, &port_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  return port_value;
}
