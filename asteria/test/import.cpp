// This file is part of Asteria.
// Copyleft 2018 - 2021, LH_Mouse. All wrongs reserved.

#include "utils.hpp"
#include "../src/simple_script.hpp"
#include "../src/runtime/global_context.hpp"

using namespace asteria;

int main()
  {
    const ::rocket::unique_ptr<char, void (&)(void*)> abspath(::realpath(__FILE__, nullptr), ::free);
    ROCKET_ASSERT(abspath);

    Simple_Script code;
    code.reload_string(
      cow_string(abspath), __LINE__, sref(R"__(
///////////////////////////////////////////////////////////////////////////////

        std.debug.logf("__file = $1", __file);
        assert import("import_sub.txt", 3, 5) == -2;
        assert import("import_sub.txt", 3, 5,) == -2;

        try { import("nonexistent file");  assert false;  }
          catch(e) { assert std.string.find(e, "assertion failure") == null;  }

        try { import("import_recursive.txt");  assert false;  }
          catch(e) { assert std.string.find(e, "recursive import") != null;  }

///////////////////////////////////////////////////////////////////////////////
      )__"));
    Global_Context global;
    code.execute(global);
  }
