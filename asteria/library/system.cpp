// This file is part of Asteria.
// Copyleft 2018 - 2023, LH_Mouse. All wrongs reserved.

#include "../precompiled.ipp"
#include "system.hpp"
#include "../argument_reader.hpp"
#include "../binding_generator.hpp"
#include "../runtime/runtime_error.hpp"
#include "../runtime/global_context.hpp"
#include "../runtime/garbage_collector.hpp"
#include "../runtime/random_engine.hpp"
#include "../compiler/token_stream.hpp"
#include "../compiler/compiler_error.hpp"
#include "../compiler/enums.hpp"
#include "../utils.hpp"
#include <spawn.h>  // ::posix_spawnp()
#include <sys/wait.h>  // ::waitpid()
#include <sys/utsname.h>  // ::uname()
#include <sys/sysinfo.h>  // ::get_nprocs()
#include <sys/socket.h>  // ::socket()
#include <time.h>  // ::clock_gettime()
namespace asteria {
namespace {

opt<Punctuator>
do_accept_punctuator_opt(Token_Stream& tstrm, initializer_list<Punctuator> accept)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok)
      return nullopt;

    if(!qtok->is_punctuator())
      return nullopt;

    auto punct = qtok->as_punctuator();
    if(::rocket::is_none_of(punct, accept))
      return nullopt;

    tstrm.shift();
    return punct;
  }

struct Xparse_array
  {
    V_array arr;
  };

struct Xparse_object
  {
    V_object obj;
    phsh_string key;
    Source_Location key_sloc;
  };

using Xparse = ::rocket::variant<Xparse_array, Xparse_object>;

void
do_accept_object_key(Xparse_object& ctxo, Token_Stream& tstrm)
  {
    auto qtok = tstrm.peek_opt();
    if(!qtok)
      throw Compiler_Error(Compiler_Error::M_status(),
                compiler_status_identifier_expected, tstrm.next_sloc());

    switch(weaken_enum(qtok->index())) {
      case Token::index_identifier:
        ctxo.key = qtok->as_identifier();
        break;

      case Token::index_string_literal:
        ctxo.key = qtok->as_string_literal();
        break;

      default:
        throw Compiler_Error(Compiler_Error::M_status(),
                  compiler_status_identifier_expected, tstrm.next_sloc());
    }

    ctxo.key_sloc = qtok->sloc();
    tstrm.shift();

    // A colon or equals sign may follow, but it has no meaning whatsoever.
    do_accept_punctuator_opt(tstrm, { punctuator_colon, punctuator_assign });
  }

Value
do_conf_parse_value_nonrecursive(Token_Stream& tstrm)
  {
    // Implement a non-recursive descent parser.
    Value value;
    cow_vector<Xparse> stack;

    // Accept a value. No other things such as closed brackets are allowed.
  parse_next:
    auto qtok = tstrm.peek_opt();
    if(!qtok)
      throw Compiler_Error(Compiler_Error::M_format(),
                compiler_status_expression_expected, tstrm.next_sloc(),
                "Value expected");

    switch(weaken_enum(qtok->index())) {
      case Token::index_punctuator:
        // Accept an `[` or `{`.
        if(qtok->as_punctuator() == punctuator_bracket_op) {
          tstrm.shift();

          auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_cl });
          if(!kpunct) {
            stack.emplace_back(Xparse_array());
            goto parse_next;
          }

          // Accept an empty array.
          value = V_array();
          break;
        }
        else if(qtok->as_punctuator() == punctuator_brace_op) {
          tstrm.shift();

          auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_brace_cl });
          if(!kpunct) {
            stack.emplace_back(Xparse_object());
            do_accept_object_key(stack.mut_back().mut<Xparse_object>(), tstrm);
            goto parse_next;
          }

          // Accept an empty object.
          value = V_object();
          break;
        }
        else
          throw Compiler_Error(Compiler_Error::M_format(),
                    compiler_status_expression_expected, tstrm.next_sloc(),
                    "Value expected");

      case Token::index_identifier:
        // Accept a literal.
        if(qtok->as_identifier() == "null") {
          tstrm.shift();
          value = nullopt;
          break;
        }
        else if(qtok->as_identifier() == "true") {
          tstrm.shift();
          value = true;
          break;
        }
        else if(qtok->as_identifier() == "false") {
          tstrm.shift();
          value = false;
          break;
        }
        else if((qtok->as_identifier() == "Infinity") || (qtok->as_identifier() == "infinity")) {
          tstrm.shift();
          value = ::std::numeric_limits<double>::infinity();
          break;
        }
        else if((qtok->as_identifier() == "NaN") || (qtok->as_identifier() == "nan")) {
          tstrm.shift();
          value = ::std::numeric_limits<double>::quiet_NaN();
          break;
        }
        else
          throw Compiler_Error(Compiler_Error::M_format(),
                    compiler_status_expression_expected, tstrm.next_sloc(),
                    "Value expected");

      case Token::index_integer_literal:
        // Accept an integer.
        value = qtok->as_integer_literal();
        tstrm.shift();
        break;

      case Token::index_real_literal:
        // Accept a real number.
        value = qtok->as_real_literal();
        tstrm.shift();
        break;

      case Token::index_string_literal:
        // Accept a UTF-8 string.
        value = qtok->as_string_literal();
        tstrm.shift();
        break;

      default:
        throw Compiler_Error(Compiler_Error::M_format(),
                  compiler_status_expression_expected, tstrm.next_sloc(),
                  "Value expected");
    }

    while(stack.size()) {
      // Advance to the next element.
      auto& ctx = stack.mut_back();
      switch(ctx.index()) {
        case 0: {
          auto& ctxa = ctx.mut<Xparse_array>();
          ctxa.arr.emplace_back(::std::move(value));

          // A comma or semicolon may follow, but it has no meaning whatsoever.
          do_accept_punctuator_opt(tstrm, { punctuator_comma, punctuator_semicol });

          // Look for the next element.
          auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_bracket_cl });
          if(!kpunct)
            goto parse_next;

          // Close this array.
          value = ::std::move(ctxa.arr);
          break;
        }

        case 1: {
          auto& ctxo = ctx.mut<Xparse_object>();
          auto pair = ctxo.obj.try_emplace(::std::move(ctxo.key), ::std::move(value));
          if(!pair.second)
            throw Compiler_Error(Compiler_Error::M_status(),
                      compiler_status_duplicate_key_in_object, ctxo.key_sloc);

          // A comma or semicolon may follow, but it has no meaning whatsoever.
          do_accept_punctuator_opt(tstrm, { punctuator_comma, punctuator_semicol });

          // Look for the next element.
          auto kpunct = do_accept_punctuator_opt(tstrm, { punctuator_brace_cl });
          if(!kpunct) {
            do_accept_object_key(stack.mut_back().mut<Xparse_object>(), tstrm);
            goto parse_next;
          }

          // Close this object.
          value = ::std::move(ctxo.obj);
          break;
        }

        default:
          ROCKET_ASSERT(false);
      }

      stack.pop_back();
    }

    return value;
  }

}  // namespace

V_integer
std_system_gc_count_variables(Global_Context& global, V_integer generation)
  {
    auto rgen = ::rocket::clamp_cast<GC_Generation>(generation, 0, 2);
    if(rgen != generation)
      ASTERIA_THROW_RUNTIME_ERROR((
          "Invalid generation `$1`"),
          generation);

    // Get the current number of variables being tracked.
    const auto gcoll = global.garbage_collector();
    size_t nvars = gcoll->count_tracked_variables(rgen);
    return static_cast<int64_t>(nvars);
  }

V_integer
std_system_gc_get_threshold(Global_Context& global, V_integer generation)
  {
    auto rgen = ::rocket::clamp_cast<GC_Generation>(generation, 0, 2);
    if(rgen != generation)
      ASTERIA_THROW_RUNTIME_ERROR((
          "Invalid generation `$1`"),
          generation);

    // Get the current number of variables being tracked.
    const auto gcoll = global.garbage_collector();
    size_t thres = gcoll->get_threshold(rgen);
    return static_cast<int64_t>(thres);
  }

V_integer
std_system_gc_set_threshold(Global_Context& global, V_integer generation, V_integer threshold)
  {
    auto rgen = ::rocket::clamp_cast<GC_Generation>(generation, 0, 2);
    if(rgen != generation)
      ASTERIA_THROW_RUNTIME_ERROR((
          "Invalid generation `$1`"),
          generation);

    // Set the threshold and return its old value.
    const auto gcoll = global.garbage_collector();
    size_t oldval = gcoll->get_threshold(rgen);
    gcoll->set_threshold(rgen, ::rocket::clamp_cast<size_t>(threshold, 0, PTRDIFF_MAX));
    return static_cast<int64_t>(oldval);
  }

V_integer
std_system_gc_collect(Global_Context& global, optV_integer generation_limit)
  {
    auto rglimit = gc_generation_oldest;
    if(generation_limit) {
      rglimit = ::rocket::clamp_cast<GC_Generation>(*generation_limit, 0, 2);
      if(rglimit != *generation_limit)
        ASTERIA_THROW_RUNTIME_ERROR((
            "Invalid generation limit `$1`"),
            *generation_limit);
    }

    // Perform garbage collection up to the generation specified.
    const auto gcoll = global.garbage_collector();
    size_t nvars = gcoll->collect_variables(rglimit);
    return static_cast<int64_t>(nvars);
  }

optV_string
std_system_env_get_variable(V_string name)
  {
    const char* val = ::secure_getenv(name.safe_c_str());
    if(!val)
      return nullopt;

    // XXX: Use `sref()`?  But environment variables may be unset!
    return cow_string(val);
  }

V_object
std_system_env_get_variables()
  {
    const char* const* vpos = ::environ;
    V_object vars;
    while(const char* str = *(vpos++)) {
      // The key is terminated by an equals sign.
      const char* equ = ::std::strchr(str, '=');
      if(ROCKET_UNEXPECT(!equ))
        vars.insert_or_assign(cow_string(str), sref(""));  // no equals sign?
      else
        vars.insert_or_assign(cow_string(str, equ), cow_string(equ + 1));
    }
    return vars;
  }

V_object
std_system_get_properties()
  {
    struct ::utsname uts;
    if(::uname(&uts) != 0)
      return { };

    // Convert the result to an `object`.
    V_object names;

    names.try_emplace(sref("os"),
      V_string(
        uts.sysname  // name of the operating system
      ));

    names.try_emplace(sref("kernel"),
      V_string(
        cow_string(uts.release) + ' ' + uts.version  // name and release of the kernel
      ));

    names.try_emplace(sref("arch"),
      V_string(
        uts.machine  // name of the CPU architecture
      ));

    names.try_emplace(sref("nprocs"),
      V_integer(
        (unsigned) ::get_nprocs()  // number of active CPU cores
      ));

    return names;
  }

V_string
std_system_uuid(Global_Context& global)
  {
    // Canonical form: `xxxxxxxx-xxxx-Myyy-Nzzz-wwwwwwwwwwww`
    //  * x: number of 1/30518 seconds since UNIX Epoch
    //  * M: always `4` (UUID version)
    //  * y: process ID
    //  * N: any of `0`-`7` (UUID variant)
    //  * z: context ID
    //  * w: random bytes
    static atomic<uint64_t> m_serial;
    const auto prng = global.random_engine();
    ::timespec ts;
    ::clock_gettime(CLOCK_REALTIME, &ts);

    uint64_t x = (uint64_t) ts.tv_sec * 30518U + (uint32_t) ts.tv_nsec / 32768U + m_serial.xadd(1U);
    uint64_t y = (uint32_t) ::getpid();
    uint64_t z = (uint64_t)(void*) &global >> 12;
    uint64_t w = (uint64_t) prng->bump() << 32 | prng->bump();

    // Set version and variant.
    y &= 0x0FFF;
    y |= 0x4000;
    z &= 0x7FFF;

    // Compose the UUID string.
    cow_string str;
    auto wpos = str.insert(str.begin(), 36, '-');

    auto put_hex_uint16 = [&](uint64_t value)
      {
        uint32_t ch;
        for(int k = 3;  k >= 0;  --k)
          ch = (uint32_t) (value >> k * 4) & 0x0F,
            *(wpos++) = (char) ('0' + ch + ((9 - ch) >> 29));
      };

    put_hex_uint16(x >> 32);
    put_hex_uint16(x >> 16);
    wpos++;
    put_hex_uint16(x);
    wpos++;
    put_hex_uint16(y);
    wpos++;
    put_hex_uint16(z);
    wpos++;
    put_hex_uint16(w >> 32);
    put_hex_uint16(w >> 32);
    put_hex_uint16(w);
    return str;
  }

V_integer
std_system_proc_get_pid()
  {
    return ::getpid();
  }

V_integer
std_system_proc_get_ppid()
  {
    return ::getppid();
  }

V_integer
std_system_proc_get_uid()
  {
    return ::getuid();
  }

V_integer
std_system_proc_get_euid()
  {
    return ::geteuid();
  }

V_integer
std_system_proc_invoke(V_string cmd, optV_array argv, optV_array envp)
  {
    // Append arguments.
    cow_vector<const char*> ptrs = { cmd.safe_c_str() };
    if(argv) {
      ::rocket::for_each(*argv,
          [&](const Value& arg) { ptrs.emplace_back(arg.as_string().safe_c_str());  });
    }
    auto eoff = ptrs.ssize();  // beginning of environment variables
    ptrs.emplace_back(nullptr);

    // Append environment variables.
    if(envp) {
      eoff = ptrs.ssize();
      ::rocket::for_each(*envp,
         [&](const Value& env) { ptrs.emplace_back(env.as_string().safe_c_str());  });
      ptrs.emplace_back(nullptr);
    }
    auto argv_pp = const_cast<char**>(ptrs.data());
    auto envp_pp = const_cast<char**>(ptrs.data() + eoff);

    // Launch the program.
    ::pid_t pid;
    if(::posix_spawnp(&pid, cmd.c_str(), nullptr, nullptr, argv_pp, envp_pp) != 0)
      ASTERIA_THROW_RUNTIME_ERROR((
          "Could not spawn process '$1'",
          "[`posix_spawnp()` failed: ${errno:full}]"),
          cmd);

    for(;;) {
      // Await its termination.
      // Note: `waitpid()` may return if the child has been stopped or continued.
      int wstat;
      if(::waitpid(pid, &wstat, 0) == -1)
        ASTERIA_THROW_RUNTIME_ERROR((
            "Error awaiting child process '$1'",
            "[`waitpid()` failed: ${errno:full}]"),
            pid);

      if(WIFEXITED(wstat))
        return WEXITSTATUS(wstat);  // exited

      if(WIFSIGNALED(wstat))
        return 128 + WTERMSIG(wstat);  // killed by a signal
    }
  }

void
std_system_proc_daemonize()
  {
    // Create a socket for overwriting standard streams in child
    // processes later.
    ::rocket::unique_posix_fd tfd(::socket(AF_UNIX, SOCK_STREAM, 0));
    if(tfd == -1)
      ASTERIA_THROW_RUNTIME_ERROR((
          "Could not create blackhole stream",
          "[`socket()` failed: ${errno:full}]"));

    // Create the CHILD process and wait.
    ::pid_t cpid = ::fork();
    if(cpid == -1)
      ASTERIA_THROW_RUNTIME_ERROR((
          "Could not create child process",
          "[`fork()` failed: ${errno:full}]"));

    if(cpid != 0) {
      // Wait for the CHILD process and forward its exit code.
      int wstatus;
      for(;;)
        if(::waitpid(cpid, &wstatus, 0) != cpid)
          continue;
        else if(WIFEXITED(wstatus))
          ::_Exit(WEXITSTATUS(wstatus));
        else if(WIFSIGNALED(wstatus))
          ::_Exit(128 + WTERMSIG(wstatus));
    }

    // The CHILD shall create a new session and become its leader. This
    // ensures that a later GRANDCHILD will not be a session leader and
    // will not unintentially gain a controlling terminal.
    ::setsid();

    // Create the GRANDCHILD process.
    cpid = ::fork();
    if(cpid == -1)
      ASTERIA_TERMINATE((
          "Could not create grandchild process",
          "[`fork()` failed: ${errno:full}]"));

    if(cpid != 0) {
      // Exit so the PARENT process will continue.
      ::_Exit(0);
    }

    // Close standard streams in the GRANDCHILD. Errors are ignored.
    // The GRANDCHILD shall continue execution.
    ::shutdown(tfd, SHUT_RDWR);
    (void)! ::dup2(tfd, STDIN_FILENO);
    (void)! ::dup2(tfd, STDOUT_FILENO);
    (void)! ::dup2(tfd, STDERR_FILENO);
  }

V_object
std_system_conf_load_file(V_string path)
  {
    // Initialize tokenizer options. Unlike JSON5, we support genuine integers
    // and single-quoted string literals.
    Compiler_Options opts;
    opts.keywords_as_identifiers = true;

    Token_Stream tstrm(opts);
    ::rocket::tinybuf_file cbuf(path.safe_c_str(), tinybuf::open_read);
    tstrm.reload(path, 1, ::std::move(cbuf));

    Xparse_object ctxo;
    while(!tstrm.empty()) {
      // Parse the stream for a key-value pair.
      do_accept_object_key(ctxo, tstrm);
      auto value = do_conf_parse_value_nonrecursive(tstrm);

      auto pair = ctxo.obj.try_emplace(::std::move(ctxo.key), ::std::move(value));
      if(!pair.second)
        throw Compiler_Error(Compiler_Error::M_status(),
                  compiler_status_duplicate_key_in_object, ctxo.key_sloc);

      // A comma or semicolon may follow, but it has no meaning whatsoever.
      do_accept_punctuator_opt(tstrm, { punctuator_comma, punctuator_semicol });
    }

    // Extract the value.
    return ::std::move(ctxo.obj);
  }

void
create_bindings_system(V_object& result, API_Version /*version*/)
  {
    result.insert_or_assign(sref("gc_count_variables"),
      ASTERIA_BINDING(
        "std.system.gc_count_variables", "generation",
        Global_Context& global, Argument_Reader&& reader)
      {
        V_integer gen;

        reader.start_overload();
        reader.required(gen);
        if(reader.end_overload())
          return (Value) std_system_gc_count_variables(global, gen);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("gc_get_threshold"),
      ASTERIA_BINDING(
        "std.system.gc_get_threshold", "generation",
        Global_Context& global, Argument_Reader&& reader)
      {
        V_integer gen;

        reader.start_overload();
        reader.required(gen);
        if(reader.end_overload())
          return (Value) std_system_gc_get_threshold(global, gen);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("gc_set_threshold"),
      ASTERIA_BINDING(
        "std.system.gc_set_threshold", "generation, threshold",
        Global_Context& global, Argument_Reader&& reader)
      {
        V_integer gen, thrs;

        reader.start_overload();
        reader.required(gen);
        reader.required(thrs);
        if(reader.end_overload())
          return (Value) std_system_gc_set_threshold(global, gen, thrs);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("gc_collect"),
      ASTERIA_BINDING(
        "std.system.gc_collect", "[generation_limit]",
        Global_Context& global, Argument_Reader&& reader)
      {
        optV_integer glim;

        reader.start_overload();
        reader.optional(glim);
        if(reader.end_overload())
          return (Value) std_system_gc_collect(global, glim);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("env_get_variable"),
      ASTERIA_BINDING(
        "std.system.env_get_variable", "name",
        Argument_Reader&& reader)
      {
        V_string name;

        reader.start_overload();
        reader.required(name);
        if(reader.end_overload())
          return (Value) std_system_env_get_variable(name);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("env_get_variables"),
      ASTERIA_BINDING(
        "std.system.env_get_variables", "",
        Argument_Reader&& reader)
      {
        reader.start_overload();
        if(reader.end_overload())
          return (Value) std_system_env_get_variables();

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("get_properties"),
      ASTERIA_BINDING(
        "std.system.get_properties", "",
        Argument_Reader&& reader)
      {
        reader.start_overload();
        if(reader.end_overload())
          return (Value) std_system_get_properties();

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("uuid"),
      ASTERIA_BINDING(
        "std.system.uuid", "",
        Global_Context& global, Argument_Reader&& reader)
      {
        reader.start_overload();
        if(reader.end_overload())
          return (Value) std_system_uuid(global);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("proc_get_pid"),
      ASTERIA_BINDING(
        "std.system.proc_get_pid", "",
        Argument_Reader&& reader)
      {
        reader.start_overload();
        if(reader.end_overload())
          return (Value) std_system_proc_get_pid();

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("proc_get_ppid"),
      ASTERIA_BINDING(
        "std.system.proc_get_ppid", "",
        Argument_Reader&& reader)
      {
        reader.start_overload();
        if(reader.end_overload())
          return (Value) std_system_proc_get_ppid();

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("proc_get_uid"),
      ASTERIA_BINDING(
        "std.system.proc_get_uid", "",
        Argument_Reader&& reader)
      {
        reader.start_overload();
        if(reader.end_overload())
          return (Value) std_system_proc_get_uid();

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("proc_get_euid"),
      ASTERIA_BINDING(
        "std.system.proc_get_euid", "",
        Argument_Reader&& reader)
      {
        reader.start_overload();
        if(reader.end_overload())
          return (Value) std_system_proc_get_euid();

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("proc_invoke"),
      ASTERIA_BINDING(
        "std.system.proc_invoke", "cmd, [argv], [envp]",
        Argument_Reader&& reader)
      {
        V_string cmd;
        optV_array argv, envp;

        reader.start_overload();
        reader.required(cmd);
        reader.optional(argv);
        reader.optional(envp);
        if(reader.end_overload())
          return (Value) std_system_proc_invoke(cmd, argv, envp);

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("proc_daemonize"),
      ASTERIA_BINDING(
        "std.system.proc_daemonize", "",
        Argument_Reader&& reader)
      {
        reader.start_overload();
        if(reader.end_overload())
          return (void) std_system_proc_daemonize();

        reader.throw_no_matching_function_call();
      });

    result.insert_or_assign(sref("conf_load_file"),
      ASTERIA_BINDING(
        "std.system.conf_load_file", "path",
        Argument_Reader&& reader)
      {
        V_string path;

        reader.start_overload();
        reader.required(path);
        if(reader.end_overload())
          return (Value) std_system_conf_load_file(path);

        reader.throw_no_matching_function_call();
      });
  }

}  // namespace asteria
