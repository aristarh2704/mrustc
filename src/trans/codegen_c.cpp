/*
 * MRustC - Rust Compiler
 * - By John Hodge (Mutabah/thePowersGang)
 *
 * trans/codegen_c.cpp
 * - Code generation emitting C code
 */
#include "codegen.hpp"
#include "mangling.hpp"
#include <fstream>
#include <hir/hir.hpp>
#include <mir/mir.hpp>

namespace {
    class CodeGenerator_C:
        public CodeGenerator
    {
        const ::HIR::Crate& m_crate;
        ::std::ofstream m_of;
    public:
        CodeGenerator_C(const ::HIR::Crate& crate, const ::std::string& outfile):
            m_crate(crate),
            m_of(outfile)
        {
        }
        
        ~CodeGenerator_C() {}

        void finalise() override
        {
        }
        
        void emit_struct(const ::HIR::GenericPath& p, const ::HIR::Struct& item) override
        {
            m_of << "struct s_" << Trans_Mangle(p) << " {\n";
            m_of << "}\n";
        }
        //virtual void emit_union(const ::HIR::GenericPath& p, const ::HIR::Union& item);
        //virtual void emit_enum(const ::HIR::GenericPath& p, const ::HIR::Enum& item);
        
        //virtual void emit_static_ext(const ::HIR::Path& p);
        //virtual void emit_static_local(const ::HIR::Path& p, const ::HIR::Static& item, const Trans_Params& params);
        
        void emit_function_ext(const ::HIR::Path& p, const ::HIR::Function& item, const Trans_Params& params) override
        {
            m_of << "extern ";
            emit_function_header(p, item, params);
            m_of << ";\n";
        }
        void emit_function_proto(const ::HIR::Path& p, const ::HIR::Function& item, const Trans_Params& params) override
        {
            emit_function_header(p, item, params);
            m_of << ";\n";
        }
        void emit_function_code(const ::HIR::Path& p, const ::HIR::Function& item, const Trans_Params& params, const ::MIR::FunctionPointer& code) override
        {
            static Span sp;
            TRACE_FUNCTION_F(p);

            emit_function_header(p, item, params);
            m_of << "\n";
            m_of << "{\n";
            // Variables
            m_of << "\t"; emit_ctype(params.monomorph(m_crate, item.m_return)); m_of << " rv;\n";
            for(unsigned int i = 0; i < code->named_variables.size(); i ++) {
                DEBUG("var" << i << " : " << code->named_variables[i]);
                m_of << "\t"; emit_ctype(code->named_variables[i]); m_of << " var" << i << ";\n";
            }
            for(unsigned int i = 0; i < code->temporaries.size(); i ++) {
                DEBUG("tmp" << i << " : " << code->temporaries[i]);
                m_of << "\t"; emit_ctype(code->temporaries[i]); m_of << " tmp" << i << ";\n";
            }
            // TODO: Code.
            for(unsigned int i = 0; i < code->blocks.size(); i ++)
            {
                TRACE_FUNCTION_F(p << " bb" << i);
            
                m_of << "bb" << i << ":\n";
                
                for(const auto& stmt : code->blocks[i].statements)
                {
                    assert( stmt.is_Drop() || stmt.is_Assign() );
                    if( stmt.is_Drop() ) {
                    }
                    else {
                        const auto& e = stmt.as_Assign();
                        m_of << "\t"; emit_lvalue(e.dst); m_of << " = ";
                        TU_MATCHA( (e.src), (ve),
                        (Use,
                            emit_lvalue(ve);
                            ),
                        (Constant,
                            ),
                        (SizedArray,
                            m_of << "{";
                            for(unsigned int j = ve.count; j --;) {
                                emit_lvalue(ve.val);
                                if( j != 0 )    m_of << ",";
                            }
                            m_of << "}";
                            ),
                        (Borrow,
                            m_of << "& "; emit_lvalue(ve.val);
                            ),
                        (Cast,
                            m_of << "("; emit_ctype(ve.type); m_of << ")"; emit_lvalue(ve.val);
                            ),
                        (BinOp,
                            emit_lvalue(ve.val_l);
                            switch(ve.op)
                            {
                            case ::MIR::eBinOp::ADD:   m_of << "+";    break;
                            case ::MIR::eBinOp::SUB:   m_of << "-";    break;
                            case ::MIR::eBinOp::MUL:   m_of << "*";    break;
                            case ::MIR::eBinOp::DIV:   m_of << "/";    break;
                            case ::MIR::eBinOp::MOD:   m_of << "%";    break;
                            
                            case ::MIR::eBinOp::BIT_OR:    m_of << "|";    break;
                            case ::MIR::eBinOp::BIT_AND:   m_of << "&";    break;
                            case ::MIR::eBinOp::BIT_XOR:   m_of << "^";    break;
                            case ::MIR::eBinOp::BIT_SHR:   m_of << ">>";   break;
                            case ::MIR::eBinOp::BIT_SHL:   m_of << "<<";   break;
                            case ::MIR::eBinOp::EQ:    m_of << "==";   break;
                            case ::MIR::eBinOp::NE:    m_of << "!=";   break;
                            case ::MIR::eBinOp::GT:    m_of << ">" ;   break;
                            case ::MIR::eBinOp::GE:    m_of << ">=";   break;
                            case ::MIR::eBinOp::LT:    m_of << "<" ;   break;
                            case ::MIR::eBinOp::LE:    m_of << "<=";   break;
                            
                            case ::MIR::eBinOp::ADD_OV:
                            case ::MIR::eBinOp::SUB_OV:
                            case ::MIR::eBinOp::MUL_OV:
                            case ::MIR::eBinOp::DIV_OV:
                                TODO(sp, "Overflow");
                                break;
                            }
                            emit_lvalue(ve.val_r);
                            ),
                        (UniOp,
                            ),
                        (DstMeta,
                            emit_lvalue(ve.val);
                            m_of << ".META";
                            ),
                        (DstPtr,
                            emit_lvalue(ve.val);
                            m_of << ".PTR";
                            ),
                        (MakeDst,
                            m_of << "{";
                            emit_lvalue(ve.ptr_val);
                            m_of << ",";
                            emit_lvalue(ve.meta_val);
                            m_of << "}";
                            ),
                        (Tuple,
                            m_of << "{";
                            for(unsigned int j = 0; j < ve.vals.size(); j ++) {
                                if( j != 0 )    m_of << ",";
                                emit_lvalue(ve.vals[j]);
                            }
                            m_of << "}";
                            ),
                        (Array,
                            m_of << "{";
                            for(unsigned int j = 0; j < ve.vals.size(); j ++) {
                                if( j != 0 )    m_of << ",";
                                emit_lvalue(ve.vals[j]);
                            }
                            m_of << "}";
                            ),
                        (Variant,
                            TODO(sp, "Handle constructing variants");
                            ),
                        (Struct,
                            m_of << "{";
                            for(unsigned int j = 0; j < ve.vals.size(); j ++) {
                                if( j != 0 )    m_of << ",";
                                emit_lvalue(ve.vals[j]);
                            }
                            m_of << "}";
                            )
                        )
                        m_of << ";\n";
                    }
                }
                TU_MATCHA( (code->blocks[i].terminator), (e),
                (Incomplete,
                    m_of << "\tfor(;;);\n";
                    ),
                (Return,
                    m_of << "\treturn rv;\n";
                    ),
                (Diverge,
                    m_of << "\t_Unwind_Resume();\n";
                    ),
                (Goto,
                    m_of << "\tgoto bb" << e << ";\n";
                    ),
                (Panic,
                    m_of << "\tgoto bb" << e << "; /* panic */\n";
                    ),
                (If,
                    m_of << "\tif("; emit_lvalue(e.cond); m_of << ") goto bb" << e.bb0 << "; else goto bb" << e.bb1 << ";\n";
                    ),
                (Switch,
                    m_of << "\tswitch("; emit_lvalue(e.val); m_of << ".TAG) {\n";
                    for(unsigned int j = 0; j < e.targets.size(); j ++)
                        m_of << "\t\tcase " << j << ": goto bb" << e.targets[j] << ";\n";
                    m_of << "\t}\n";
                    ),
                (CallValue,
                    ),
                (CallPath,
                    )
                )
            }
            m_of << "}\n";
            m_of.flush();
        }
    private:
        void emit_function_header(const ::HIR::Path& p, const ::HIR::Function& item, const Trans_Params& params)
        {
            emit_ctype( params.monomorph(m_crate, item.m_return) );
            m_of << " " << Trans_Mangle(p) << "(";
            for(unsigned int i = 0; i < item.m_args.size(); i ++)
            {
                if( i != 0 )    m_of << ", ";
                emit_ctype( params.monomorph(m_crate, item.m_args[i].second) );
                m_of << " arg" << i;
            }
            m_of << ")";
        }
        void emit_lvalue(const ::MIR::LValue& val) {
        }
        void emit_ctype(const ::HIR::TypeRef& ty) {
            TU_MATCHA( (ty.m_data), (te),
            (Infer,
                m_of << "@" << ty << "@";
                ),
            (Diverge,
                m_of << "void";
                ),
            (Primitive,
                switch(te)
                {
                case ::HIR::CoreType::Usize:    m_of << "size_t";   break;
                case ::HIR::CoreType::Isize:    m_of << "ssize_t";  break;
                case ::HIR::CoreType::U8:  m_of << "uint8_t"; break;
                case ::HIR::CoreType::I8:  m_of << "int8_t"; break;
                case ::HIR::CoreType::U16: m_of << "uint16_t"; break;
                case ::HIR::CoreType::I16: m_of << "int16_t"; break;
                case ::HIR::CoreType::U32: m_of << "uint32_t"; break;
                case ::HIR::CoreType::I32: m_of << "int32_t"; break;
                case ::HIR::CoreType::U64: m_of << "uint64_t"; break;
                case ::HIR::CoreType::I64: m_of << "int64_t"; break;
                
                case ::HIR::CoreType::F32: m_of << "float"; break;
                case ::HIR::CoreType::F64: m_of << "double"; break;
                
                case ::HIR::CoreType::Bool: m_of << "bool"; break;
                case ::HIR::CoreType::Char: m_of << "uin32_t";  break;
                case ::HIR::CoreType::Str:
                    BUG(Span(), "Raw str");
                }
                ),
            (Path,
                TU_MATCHA( (te.binding), (tpb),
                (Struct,
                    m_of << "struct s_" << Trans_Mangle(te.path);
                    ),
                (Union,
                    m_of << "struct e_" << Trans_Mangle(te.path);
                    ),
                (Enum,
                    m_of << "union u_" << Trans_Mangle(te.path);
                    ),
                (Unbound,
                    BUG(Span(), "Unbound path in trans - " << ty);
                    ),
                (Opaque,
                    BUG(Span(), "Opaque path in trans - " << ty);
                    )
                )
                ),
            (Generic,
                BUG(Span(), "Generic in trans - " << ty);
                ),
            (TraitObject,
                BUG(Span(), "Raw trait object - " << ty);
                ),
            (ErasedType,
                BUG(Span(), "ErasedType in trans - " << ty);
                ),
            (Array,
                m_of << "("; emit_ctype(*te.inner); m_of << ")[" << te.size_val << "]";
                ),
            (Slice,
                BUG(Span(), "Raw slice object - " << ty);
                ),
            (Tuple,
                if( te.size() == 0 )
                    m_of << "tUNIT";
                else {
                    m_of << "TUP_" << te.size();
                    for(const auto& t : te)
                        m_of << "_" << Trans_Mangle(t);
                }
                ),
            (Borrow,
                if( *te.inner == ::HIR::CoreType::Str ) {
                    m_of << "STR_PTR";
                }
                else if( te.inner->m_data.is_TraitObject() ) {
                    m_of << "TRAITOBJ_PTR";
                }
                else if( te.inner->m_data.is_Slice() ) {
                    m_of << "SLICE_PTR";
                }
                else {
                    emit_ctype(*te.inner);
                    m_of << "*";
                }
                ),
            (Pointer,
                if( *te.inner == ::HIR::CoreType::Str ) {
                    m_of << "STR_PTR";
                }
                else if( te.inner->m_data.is_TraitObject() ) {
                    m_of << "TRAITOBJ_PTR";
                }
                else if( te.inner->m_data.is_Slice() ) {
                    m_of << "SLICE_PTR";
                }
                else {
                    emit_ctype(*te.inner);
                    m_of << "*";
                }
                ),
            (Function,
                m_of << "void(void)*";
                //TODO(Span(), "Emit function pointer type - " << ty);
                ),
            (Closure,
                BUG(Span(), "Closure during trans - " << ty);
                )
            )
        }
    };
}

::std::unique_ptr<CodeGenerator> Trans_Codegen_GetGeneratorC(const ::HIR::Crate& crate, const ::std::string& outfile)
{
    return ::std::unique_ptr<CodeGenerator>(new CodeGenerator_C(crate, outfile));
}
