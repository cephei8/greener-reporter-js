#include "reporter.hpp"

#include <greener_reporter/greener_reporter.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

namespace {
void handle_reporter_error(napi_env env, const greener_reporter_error *err) {
  std::stringstream ss;
  ss << "GreenerReporterError " << err->code << "/" << err->ingress_code << ": "
     << err->message;
  const std::string &msg = ss.str();
  napi_throw_error(env, nullptr, msg.c_str());
  greener_reporter_error_delete(err);
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

reporter::~reporter() {
  if (reporter_) {
    const greener_reporter_error *err;
    greener_reporter_delete(reporter_, &err);
  }
  napi_delete_reference(env_, wrapper_);
}

napi_value reporter::register_class(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
      {"createSession", nullptr, create_session, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"createTestcase", nullptr, create_testcase, nullptr, nullptr, nullptr,
       napi_default, nullptr},
      {"shutdown", nullptr, shutdown, nullptr, nullptr, nullptr, napi_default,
       nullptr},
  };

  napi_value ctor_value;
  if (const auto status =
          napi_define_class(env, "Reporter", NAPI_AUTO_LENGTH, ctor, nullptr,
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
          napi_set_named_property(env, exports, "Reporter", ctor_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  return exports;
}

napi_value reporter::ctor(napi_env env, napi_callback_info info) {
  napi_value target;
  if (const auto status = napi_get_new_target(env, info, &target);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  constexpr size_t argc_num = 2;
  size_t argc = argc_num;
  napi_value args[argc_num];
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 2) {
    napi_throw_error(env, nullptr, "invalid number of arguments");
    return nullptr;
  }

  // endpoint
  std::string server_address;
  {
    char buf[256];
    size_t buf_len;

    napi_valuetype arg_type;
    if (const auto status = napi_typeof(env, args[0], &arg_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }
    if (arg_type != napi_string) {
      napi_throw_error(env, nullptr, "server_address must be a string");
      return nullptr;
    }
    if (const auto status = napi_get_value_string_utf8(
            env, args[0], buf, std::size(buf), &buf_len);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    server_address = std::string(buf, buf_len);
  }

  // api key
  std::string api_key;
  {
    char buf[256];
    size_t buf_len;

    napi_valuetype arg_type;
    if (const auto status = napi_typeof(env, args[1], &arg_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }
    if (arg_type != napi_string) {
      napi_throw_error(env, nullptr, "apiKey must be a string");
      return nullptr;
    }
    if (const auto status = napi_get_value_string_utf8(
            env, args[1], buf, std::size(buf), &buf_len);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    api_key = std::string(buf, buf_len);
  }

  auto *obj = new reporter();
  obj->env_ = env;
  if (const auto status =
          napi_wrap(env, jsthis, obj, finalizer, nullptr, &obj->wrapper_);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_reporter_error *err;
  obj->reporter_ =
      greener_reporter_new(server_address.c_str(), api_key.c_str(), &err);
  if (err != nullptr) {
    handle_reporter_error(env, err);
    return nullptr;
  }

  return jsthis;
}

void reporter::finalizer(napi_env env, void *obj, void *) {
  static_cast<reporter *>(obj)->~reporter();
}

napi_value reporter::create_session(napi_env env, napi_callback_info info) {
  constexpr size_t argc_num = 4;
  size_t argc = argc_num;
  napi_value args[argc_num];
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 4) {
    napi_throw_error(env, nullptr, "invalid number of arguments");
    return nullptr;
  }

  std::optional<std::string> in_session_id;
  std::optional<std::string> in_description;
  std::optional<std::string> in_baggage;
  std::optional<std::string> in_labels;
  // id
  {
    napi_valuetype in_type;
    if (const auto status = napi_typeof(env, args[0], &in_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(in_type == napi_string || in_type == napi_null)) {
      napi_throw_error(env, nullptr, "session_id must be a nullable string");
      return nullptr;
    }

    if (in_type == napi_string) {
      char buf[256];
      size_t buf_len;

      if (const auto status = napi_get_value_string_utf8(
              env, args[0], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_session_id = std::string(buf, buf_len);
    }
  }

  // description
  {
    constexpr size_t arg_idx = 1;
    napi_valuetype in_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &in_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(in_type == napi_string || in_type == napi_null)) {
      napi_throw_error(env, nullptr, "description must be a nullable string");
      return nullptr;
    }

    if (in_type == napi_string) {
      char buf[256];
      size_t buf_len;

      if (const auto status = napi_get_value_string_utf8(
              env, args[arg_idx], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_description = std::string(buf, buf_len);
    }
  }

  // baggage
  {
    constexpr size_t arg_idx = 2;
    napi_valuetype in_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &in_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(in_type == napi_string || in_type == napi_null)) {
      napi_throw_error(env, nullptr, "baggage must be a nullable string");
      return nullptr;
    }

    if (in_type == napi_string) {
      char buf[256];
      size_t buf_len;

      if (const auto status = napi_get_value_string_utf8(
              env, args[arg_idx], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_baggage = std::string(buf, buf_len);
    }
  }

  // labels
  {
    constexpr size_t arg_idx = 3;
    napi_valuetype in_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &in_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(in_type == napi_string || in_type == napi_null)) {
      napi_throw_error(env, nullptr, "labels must be a nullable string");
      return nullptr;
    }

    if (in_type == napi_string) {
      char buf[256];
      size_t buf_len;

      if (const auto status = napi_get_value_string_utf8(
              env, args[arg_idx], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_labels = std::string(buf, buf_len);
    }
  }

  reporter *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  auto session_deleter = [](const greener_reporter_session *s) {
    greener_reporter_session_delete(s);
  };
  const greener_reporter_error *err;
  std::unique_ptr<const greener_reporter_session, decltype(session_deleter)>
      session(greener_reporter_session_create(
                  obj->reporter_,
                  in_session_id.has_value() ? in_session_id.value().c_str()
                                            : nullptr,
                  in_description.has_value() ? in_description.value().c_str()
                                             : nullptr,
                  in_baggage.has_value() ? in_baggage.value().c_str() : nullptr,
                  in_labels.has_value() ? in_labels.value().c_str() : nullptr,
                  &err),
              session_deleter);

  if (err != nullptr) {
    handle_reporter_error(env, err);
    return nullptr;
  }

  const auto *out_session_id = session->id;

  napi_value out_session_id_value;
  if (const auto status = napi_create_string_utf8(
          env, out_session_id, strlen(out_session_id), &out_session_id_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  napi_value out_session_value;
  if (const auto status = napi_create_object(env, &out_session_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (const auto status = napi_set_named_property(env, out_session_value, "id",
                                                  out_session_id_value);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  return out_session_value;
}

napi_value reporter::create_testcase(napi_env env, napi_callback_info info) {
  constexpr size_t argc_num = 8;
  size_t argc = argc_num;
  napi_value args[argc_num];
  napi_value jsthis;
  if (const auto status =
          napi_get_cb_info(env, info, &argc, args, &jsthis, nullptr);
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  if (argc != 8) {
    napi_throw_error(env, nullptr, "invalid number of arguments");
    return nullptr;
  }

  std::string in_session_id;
  std::string in_testcase_name;
  std::optional<std::string> in_testcase_classname;
  std::optional<std::string> in_testcase_file;
  std::optional<std::string> in_testsuite;
  std::string in_status;
  std::optional<std::string> in_output;
  std::optional<std::string> in_baggage;

  // session_id
  {
    const size_t arg_idx = 0;
    char buf[256];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (value_type != napi_string) {
      napi_throw_error(env, nullptr, "session_id must be a string");
      return nullptr;
    }
    if (const auto status = napi_get_value_string_utf8(
            env, args[arg_idx], buf, std::size(buf), &buf_len);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    in_session_id = std::string(buf, buf_len);
  }

  // testcase_name
  {
    const size_t arg_idx = 1;
    char buf[256];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (value_type != napi_string) {
      napi_throw_error(env, nullptr, "testcase_name must be a string");
      return nullptr;
    }
    if (const auto status = napi_get_value_string_utf8(
            env, args[arg_idx], buf, std::size(buf), &buf_len);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    in_testcase_name = std::string(buf, buf_len);
  }

  // testcase_classname
  {
    const size_t arg_idx = 2;
    char buf[256];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, nullptr,
                       "testcase_classname must be a nullable string");
      return nullptr;
    }

    if (value_type == napi_string) {
      if (const auto status = napi_get_value_string_utf8(
              env, args[arg_idx], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_testcase_classname = std::string(buf, buf_len);
    }
  }

  // testcase_file
  {
    const size_t arg_idx = 3;
    char buf[256];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, nullptr, "testcase_file must be a nullable string");
      return nullptr;
    }

    if (value_type == napi_string) {
      if (const auto status = napi_get_value_string_utf8(
              env, args[arg_idx], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_testcase_file = std::string(buf, buf_len);
    }
  }

  // testsuite
  {
    const size_t arg_idx = 4;
    char buf[256];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, nullptr, "testsuite must be a nullable string");
      return nullptr;
    }

    if (value_type == napi_string) {
      if (const auto status = napi_get_value_string_utf8(
              env, args[arg_idx], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_testsuite = std::string(buf, buf_len);
    }
  }

  // status
  {
    const size_t arg_idx = 5;
    char buf[256];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (value_type != napi_string) {
      napi_throw_error(env, nullptr, "status must be a string");
      return nullptr;
    }
    if (const auto status = napi_get_value_string_utf8(
            env, args[arg_idx], buf, std::size(buf), &buf_len);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    in_status = std::string(buf, buf_len);

    if (in_status != "pass" && in_status != "fail" && in_status != "error" &&
        in_status != "skip") {
      napi_throw_error(env, nullptr,
                       "status must be one of pass|fail|err|skip");
      return nullptr;
    }
  }

  // output
  {
    const size_t arg_idx = 6;
    char buf[256];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, nullptr, "stdout_stream must be a nullable string");
      return nullptr;
    }

    if (value_type == napi_string) {
      if (const auto status = napi_get_value_string_utf8(
              env, args[arg_idx], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_output = std::string(buf, buf_len);
    }
  }

  // baggage
  {
    const size_t arg_idx = 7;
    char buf[256];
    size_t buf_len;

    napi_valuetype value_type;
    if (const auto status = napi_typeof(env, args[arg_idx], &value_type);
        status != napi_ok) {
      handle_napi_error(env);
      return nullptr;
    }

    if (!(value_type == napi_string || value_type == napi_null)) {
      napi_throw_error(env, nullptr, "baggage must be a nullable string");
      return nullptr;
    }

    if (value_type == napi_string) {
      if (const auto status = napi_get_value_string_utf8(
              env, args[arg_idx], buf, std::size(buf), &buf_len);
          status != napi_ok) {
        handle_napi_error(env);
        return nullptr;
      }

      in_baggage = std::string(buf, buf_len);
    }
  }

  reporter *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_reporter_error *err;
  greener_reporter_testcase_create(
      obj->reporter_, in_session_id.c_str(), in_testcase_name.c_str(),
      in_testcase_classname.has_value() ? in_testcase_classname.value().c_str()
                                        : nullptr,
      in_testcase_file.has_value() ? in_testcase_file.value().c_str() : nullptr,
      in_testsuite.has_value() ? in_testsuite.value().c_str() : nullptr,
      in_status.c_str(),
      in_output.has_value() ? in_output.value().c_str() : nullptr,
      in_baggage.has_value() ? in_baggage.value().c_str() : nullptr, &err);

  if (err != nullptr) {
    handle_reporter_error(env, err);
    return nullptr;
  }

  return nullptr;
}

napi_value reporter::shutdown(napi_env env, napi_callback_info info) {
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

  reporter *obj;
  if (const auto status =
          napi_unwrap(env, jsthis, reinterpret_cast<void **>(&obj));
      status != napi_ok) {
    handle_napi_error(env);
    return nullptr;
  }

  const greener_reporter_error *err;
  greener_reporter_delete(obj->reporter_, &err);
  obj->reporter_ = nullptr;

  if (err != nullptr) {
    handle_reporter_error(env, err);
    return nullptr;
  }

  return nullptr;
}
