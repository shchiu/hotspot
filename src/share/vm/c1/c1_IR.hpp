/*
 * Copyright (c) 1999, 2009, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

// An XHandler is a C1 internal description for an exception handler

class XHandler: public CompilationResourceObj {
 private:
  ciExceptionHandler* _desc;

  BlockBegin*         _entry_block;  // Entry block of xhandler
  LIR_List*           _entry_code;   // LIR-operations that must be executed before jumping to entry_block
  int                 _entry_pco;    // pco where entry_code (or entry_block if no entry_code) starts
  int                 _phi_operand;  // For resolving of phi functions at begin of entry_block
  int                 _scope_count;  // for filling ExceptionRangeEntry::scope_count

#ifdef ASSERT
  int                 _lir_op_id;    // op_id of the LIR-operation throwing to this handler
#endif

 public:
  // creation
  XHandler(ciExceptionHandler* desc)
    : _desc(desc)
    , _entry_block(NULL)
    , _entry_code(NULL)
    , _entry_pco(-1)
    , _phi_operand(-1)
    , _scope_count(-1)
#ifdef ASSERT
    , _lir_op_id(-1)
#endif
  { }

  XHandler(XHandler* other)
    : _desc(other->_desc)
    , _entry_block(other->_entry_block)
    , _entry_code(other->_entry_code)
    , _entry_pco(other->_entry_pco)
    , _phi_operand(other->_phi_operand)
    , _scope_count(other->_scope_count)
#ifdef ASSERT
    , _lir_op_id(other->_lir_op_id)
#endif
  { }

  // accessors for data of ciExceptionHandler
  int  beg_bci() const                           { return _desc->start(); }
  int  end_bci() const                           { return _desc->limit(); }
  int  handler_bci() const                       { return _desc->handler_bci(); }
  bool is_catch_all() const                      { return _desc->is_catch_all(); }
  int  catch_type() const                        { return _desc->catch_klass_index(); }
  ciInstanceKlass* catch_klass() const           { return _desc->catch_klass(); }
  bool covers(int bci) const                     { return beg_bci() <= bci && bci < end_bci(); }

  // accessors for additional fields
  BlockBegin* entry_block() const                { return _entry_block; }
  LIR_List*   entry_code() const                 { return _entry_code; }
  int         entry_pco() const                  { return _entry_pco; }
  int         phi_operand() const                { assert(_phi_operand != -1, "not set"); return _phi_operand; }
  int         scope_count() const                { assert(_scope_count != -1, "not set"); return _scope_count; }
  DEBUG_ONLY(int lir_op_id() const               { return _lir_op_id; });

  void set_entry_block(BlockBegin* entry_block) {
    assert(entry_block->is_set(BlockBegin::exception_entry_flag), "must be an exception handler entry");
    assert(entry_block->bci() == handler_bci(), "bci's must correspond");
    _entry_block = entry_block;
  }
  void set_entry_code(LIR_List* entry_code)      { _entry_code = entry_code; }
  void set_entry_pco(int entry_pco)              { _entry_pco = entry_pco; }
  void set_phi_operand(int phi_operand)          { _phi_operand = phi_operand; }
  void set_scope_count(int scope_count)          { _scope_count = scope_count; }
  DEBUG_ONLY(void set_lir_op_id(int lir_op_id)   { _lir_op_id = lir_op_id; });

  bool equals(XHandler* other) const;
};

define_array(_XHandlerArray, XHandler*)
define_stack(_XHandlerList, _XHandlerArray)


// XHandlers is the C1 internal list of exception handlers for a method
class XHandlers: public CompilationResourceObj {
 private:
  _XHandlerList    _list;

 public:
  // creation
  XHandlers() : _list()                          { }
  XHandlers(ciMethod* method);
  XHandlers(XHandlers* other);

  // accessors
  int       length() const                       { return _list.length(); }
  XHandler* handler_at(int i) const              { return _list.at(i); }
  bool      has_handlers() const                 { return _list.length() > 0; }
  void      append(XHandler* h)                  { _list.append(h); }
  XHandler* remove_last()                        { return _list.pop(); }

  bool      could_catch(ciInstanceKlass* klass, bool type_is_exact) const;
  bool      equals(XHandlers* others) const;
};


class IRScope;
define_array(IRScopeArray, IRScope*)
define_stack(IRScopeList, IRScopeArray)

class Compilation;
class IRScope: public CompilationResourceObj {
 private:
  // hierarchy
  Compilation*  _compilation;                    // the current compilation
  IRScope*      _caller;                         // the caller scope, or NULL
  int           _caller_bci;                     // the caller bci of the corresponding (inlined) invoke, or < 0
  ValueStack*   _caller_state;                   // the caller state, or NULL
  int           _level;                          // the inlining level
  ciMethod*     _method;                         // the corresponding method
  IRScopeList   _callees;                        // the inlined method scopes

  // graph
  XHandlers*    _xhandlers;                      // the exception handlers
  int           _number_of_locks;                // the number of monitor lock slots needed
  bool          _monitor_pairing_ok;             // the monitor pairing info
  BlockBegin*   _start;                          // the start block, successsors are method entries

  // lock stack management
  int           _lock_stack_size;                // number of expression stack elements which, if present,
                                                 // must be spilled to the stack because of exception
                                                 // handling inside inlined methods

  BitMap        _requires_phi_function;          // bit is set if phi functions at loop headers are necessary for a local variable

  // helper functions
  BlockBegin* header_block(BlockBegin* entry, BlockBegin::Flag f, ValueStack* state);
  BlockBegin* build_graph(Compilation* compilation, int osr_bci);

 public:
  // creation
  IRScope(Compilation* compilation, IRScope* caller, int caller_bci, ciMethod* method, int osr_bci, bool create_graph = false);

  // accessors
  Compilation*  compilation() const              { return _compilation; }
  IRScope*      caller() const                   { return _caller; }
  int           caller_bci() const               { return _caller_bci; }
  ValueStack*   caller_state() const             { return _caller_state; }
  int           level() const                    { return _level; }
  ciMethod*     method() const                   { return _method; }
  int           max_stack() const;               // NOTE: expensive
  int           lock_stack_size() const          {
    assert(_lock_stack_size != -1, "uninitialized");
    return _lock_stack_size;
  }
  BitMap&       requires_phi_function()          { return _requires_phi_function; }

  // mutators
  // Needed because caller state is not ready at time of IRScope construction
  void          set_caller_state(ValueStack* state) { _caller_state = state; }
  // Needed because caller state changes after IRScope construction.
  // Computes number of expression stack elements whose state must be
  // preserved in the case of an exception; these may be seen by
  // caller scopes. Zero when inlining of methods containing exception
  // handlers is disabled, otherwise a conservative approximation.
  void          compute_lock_stack_size();

  // hierarchy
  bool          is_top_scope() const             { return _caller == NULL; }
  void          add_callee(IRScope* callee)      { _callees.append(callee); }
  int           number_of_callees() const        { return _callees.length(); }
  IRScope*      callee_no(int i) const           { return _callees.at(i); }
  int           top_scope_bci() const;

  // accessors, graph
  bool          is_valid() const                 { return start() != NULL; }
  XHandlers*    xhandlers() const                { return _xhandlers; }
  int           number_of_locks() const          { return _number_of_locks; }
  void          set_min_number_of_locks(int n)   { if (n > _number_of_locks) _number_of_locks = n; }
  bool          monitor_pairing_ok() const       { return _monitor_pairing_ok; }
  BlockBegin*   start() const                    { return _start; }
};


//
// IRScopeDebugInfo records the debug information for a particular IRScope
// in a particular CodeEmitInfo.  This allows the information to be computed
// once early enough for the OopMap to be available to the LIR and also to be
// reemited for different pcs using the same CodeEmitInfo without recomputing
// everything.
//

class IRScopeDebugInfo: public CompilationResourceObj {
 private:
  IRScope*                      _scope;
  int                           _bci;
  GrowableArray<ScopeValue*>*   _locals;
  GrowableArray<ScopeValue*>*   _expressions;
  GrowableArray<MonitorValue*>* _monitors;
  IRScopeDebugInfo*             _caller;

 public:
  IRScopeDebugInfo(IRScope*                      scope,
                   int                           bci,
                   GrowableArray<ScopeValue*>*   locals,
                   GrowableArray<ScopeValue*>*   expressions,
                   GrowableArray<MonitorValue*>* monitors,
                   IRScopeDebugInfo*             caller):
      _scope(scope)
    , _locals(locals)
    , _bci(bci)
    , _expressions(expressions)
    , _monitors(monitors)
    , _caller(caller) {}


  IRScope*                      scope()       { return _scope;       }
  int                           bci()         { return _bci;         }
  GrowableArray<ScopeValue*>*   locals()      { return _locals;      }
  GrowableArray<ScopeValue*>*   expressions() { return _expressions; }
  GrowableArray<MonitorValue*>* monitors()    { return _monitors;    }
  IRScopeDebugInfo*             caller()      { return _caller;      }

  //Whether we should reexecute this bytecode for deopt
  bool should_reexecute();

  void record_debug_info(DebugInformationRecorder* recorder, int pc_offset, bool topmost) {
    if (caller() != NULL) {
      // Order is significant:  Must record caller first.
      caller()->record_debug_info(recorder, pc_offset, false/*topmost*/);
    }
    DebugToken* locvals = recorder->create_scope_values(locals());
    DebugToken* expvals = recorder->create_scope_values(expressions());
    DebugToken* monvals = recorder->create_monitor_values(monitors());
    // reexecute allowed only for the topmost frame
    bool reexecute = topmost ? should_reexecute() : false;
    bool is_method_handle_invoke = false;
    bool return_oop = false; // This flag will be ignored since it used only for C2 with escape analysis.
    recorder->describe_scope(pc_offset, scope()->method(), bci(), reexecute, is_method_handle_invoke, return_oop, locvals, expvals, monvals);
  }
};


class CodeEmitInfo: public CompilationResourceObj {
  friend class LinearScan;
 private:
  IRScopeDebugInfo* _scope_debug_info;
  IRScope*          _scope;
  XHandlers*        _exception_handlers;
  OopMap*           _oop_map;
  ValueStack*       _stack;                      // used by deoptimization (contains also monitors
  int               _bci;
  CodeEmitInfo*     _next;
  int               _id;

  FrameMap*     frame_map() const                { return scope()->compilation()->frame_map(); }
  Compilation*  compilation() const              { return scope()->compilation(); }

 public:

  // use scope from ValueStack
  CodeEmitInfo(int bci, ValueStack* stack, XHandlers* exception_handlers);

  // used by natives
  CodeEmitInfo(IRScope* scope, int bci)
    : _scope(scope)
    , _bci(bci)
    , _oop_map(NULL)
    , _scope_debug_info(NULL)
    , _stack(NULL)
    , _exception_handlers(NULL)
    , _next(NULL)
    , _id(-1) {
  }

  // make a copy
  CodeEmitInfo(CodeEmitInfo* info, bool lock_stack_only = false);

  // accessors
  OopMap* oop_map()                              { return _oop_map; }
  ciMethod* method() const                       { return _scope->method(); }
  IRScope* scope() const                         { return _scope; }
  XHandlers* exception_handlers() const          { return _exception_handlers; }
  ValueStack* stack() const                      { return _stack; }
  int bci() const                                { return _bci; }

  void add_register_oop(LIR_Opr opr);
  void record_debug_info(DebugInformationRecorder* recorder, int pc_offset);

  CodeEmitInfo* next() const        { return _next; }
  void set_next(CodeEmitInfo* next) { _next = next; }

  int id() const      { return _id; }
  void set_id(int id) { _id = id; }
};


class IR: public CompilationResourceObj {
 private:
  Compilation*     _compilation;                 // the current compilation
  IRScope*         _top_scope;                   // the root of the scope hierarchy
  WordSize         _locals_size;                 // the space required for all locals
  int              _num_loops;                   // Total number of loops
  BlockList*       _code;                        // the blocks in code generation order w/ use counts

 public:
  // creation
  IR(Compilation* compilation, ciMethod* method, int osr_bci);

  // accessors
  bool             is_valid() const              { return top_scope()->is_valid(); }
  Compilation*     compilation() const           { return _compilation; }
  IRScope*         top_scope() const             { return _top_scope; }
  int              number_of_locks() const       { return top_scope()->number_of_locks(); }
  ciMethod*        method() const                { return top_scope()->method(); }
  BlockBegin*      start() const                 { return top_scope()->start(); }
  BlockBegin*      std_entry() const             { return start()->end()->as_Base()->std_entry(); }
  BlockBegin*      osr_entry() const             { return start()->end()->as_Base()->osr_entry(); }
  WordSize         locals_size() const           { return _locals_size; }
  int              locals_size_in_words() const  { return in_words(_locals_size); }
  BlockList*       code() const                  { return _code; }
  int              num_loops() const             { return _num_loops; }
  int              max_stack() const             { return top_scope()->max_stack(); } // expensive

  // ir manipulation
  void optimize();
  void compute_predecessors();
  void split_critical_edges();
  void compute_code();
  void compute_use_counts();

  // The linear-scan order and the code emission order are equal, but
  // this may change in future
  BlockList* linear_scan_order() {  assert(_code != NULL, "not computed"); return _code; }

  // iteration
  void iterate_preorder   (BlockClosure* closure);
  void iterate_postorder  (BlockClosure* closure);
  void iterate_linear_scan_order(BlockClosure* closure);

  // debugging
  static void print(BlockBegin* start, bool cfg_only, bool live_only = false) PRODUCT_RETURN;
  void print(bool cfg_only, bool live_only = false)                           PRODUCT_RETURN;
  void verify()                                                               PRODUCT_RETURN;
};


// Globally do instruction substitution and remove substituted
// instructions from the instruction list.
//

class SubstitutionResolver: public BlockClosure {
  static void substitute(Value* v);

 public:
  SubstitutionResolver(IR* hir) {
    hir->iterate_preorder(this);
  }

  SubstitutionResolver(BlockBegin* block) {
    block->iterate_preorder(this);
  }

  virtual void block_do(BlockBegin* block);
};
