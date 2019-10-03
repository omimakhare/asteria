// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "simple_script.hpp"
#include "air_node.hpp"
#include "../compiler/token_stream.hpp"
#include "../compiler/statement_sequence.hpp"
#include "../utilities.hpp"
#include <fstream>

namespace Asteria {

Simple_Script& Simple_Script::reload(std::streambuf& sbuf, const cow_string& name)
  {
    AIR_Node::S_instantiate_function xnode = { };
    xnode.opts = this->m_opts;
    // Tokenize the character stream.
    Token_Stream tstrm(sbuf, name, xnode.opts);
    // Parse tokens.
    Statement_Sequence stmseq(tstrm, xnode.opts);
    // Initialize arguments for the function object.
    xnode.sloc = std::make_pair(name, 1);
    xnode.name = rocket::sref("<file scope>");
    xnode.params.emplace_back(rocket::sref("..."));
    xnode.body = stmseq.get_statements();
    // Construct an IR node so we can reuse its code somehow.
    AIR_Node node(rocket::move(xnode));
    ASTERIA_DEBUG_LOG("Instantiating code from \'", name, "\'...");
    auto qtarget = node.instantiate_function(nullptr);
    ASTERIA_DEBUG_LOG("Finished instantiating code from \'", name, "\' as `", *qtarget, "`.");
    // Accept it.
    this->m_cptr = rocket::move(qtarget);
    return *this;
  }

Simple_Script& Simple_Script::reload(std::streambuf* sbuf_opt, const cow_string& name)
  {
    // Read from `*sbuf_opt` if it is valid.
    // Throw an exception if no streambuf is associated.
    if(!sbuf_opt) {
      throw Parser_Error(parser_status_null_streambuf_pointer);
    }
    return this->reload(*sbuf_opt, name);
  }

Simple_Script& Simple_Script::reload_string(const cow_string& code, const cow_string& name)
  {
    // Use a `streambuf` in place of an `istream` to minimize overheads.
    cow_stringbuf sbuf;
    sbuf.set_string(code, std::ios_base::in);
    return this->reload(sbuf, name);
  }

Simple_Script& Simple_Script::reload_file(const cow_string& path)
  {
    // Open the file designated by `path`.
    // Throw an exception if the file could not be opened.
    std::filebuf sbuf;
    if(!sbuf.open(path.c_str(), std::ios_base::in)) {
      throw Parser_Error(parser_status_ifstream_open_failure);
    }
    return this->reload(sbuf, path);
  }

rcptr<Abstract_Function> Simple_Script::copy_function_opt() const noexcept
  {
    return rocket::dynamic_pointer_cast<Abstract_Function>(this->m_cptr);
  }

Reference Simple_Script::execute(const Global_Context& global, cow_vector<Reference>&& args) const
  {
    auto qtarget = this->copy_function_opt();
    if(!qtarget) {
      ASTERIA_THROW_RUNTIME_ERROR("No code has been loaded so far.");
    }
    Reference self;
    qtarget->invoke(self, global, rocket::move(args));
    self.finish_call(global);
    return self;
  }

Reference Simple_Script::execute(const Global_Context& global, cow_vector<Value>&& vals) const
  {
    cow_vector<Reference> args;
    args.resize(vals.size());
    for(size_t i = 0; i != args.size(); ++i) {
      Reference_Root::S_temporary xref = { vals[i] };
      args.mut(i) = rocket::move(xref);
    }
    return this->execute(global, rocket::move(args));
  }

Reference Simple_Script::execute(const Global_Context& global) const
  {
    return this->execute(global, cow_vector<Reference>());
  }

}  // namespace Asteria
