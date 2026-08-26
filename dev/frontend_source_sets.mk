# Per-tool implementation source lists for the compiler.
#
# Add dev/src/foo.cpp to the tools that use it by adding `foo` below. For
# subdirectories, use the path without `.cpp`, such as `parser/foo`.

FRONTEND_SOURCE_SET_TARGETS := abimangle pptoken posttoken ctrlexpr macro preproc recog nsdecl nsinit cy86 cppgm++ lowiropt lowir2cy86 lowir2native

FRONTEND_OBJ_BASENAMES_abimangle := abi_mangle
FRONTEND_OBJ_BASENAMES_pptoken := pp_tokenizer
FRONTEND_OBJ_BASENAMES_posttoken := pp_tokenizer posttoken
FRONTEND_OBJ_BASENAMES_ctrlexpr := pp_tokenizer posttoken ctrlexpr
FRONTEND_OBJ_BASENAMES_macro := pp_tokenizer posttoken macro
FRONTEND_OBJ_BASENAMES_preproc := pp_tokenizer posttoken ctrlexpr macro preproc_session
FRONTEND_OBJ_BASENAMES_recog := pp_tokenizer posttoken ctrlexpr macro preproc_session cpp_declaration_syntax pa6_recognizer pa6_parser pa6_parser_declarations pa6_parser_expressions
FRONTEND_OBJ_BASENAMES_nsdecl := pp_tokenizer posttoken ctrlexpr macro preproc_session cpp_declaration_syntax pa7_semantic
FRONTEND_OBJ_BASENAMES_nsinit := pp_tokenizer posttoken ctrlexpr macro preproc_session cpp_declaration_syntax pa8_semantic
FRONTEND_OBJ_BASENAMES_cy86 := pp_tokenizer posttoken ctrlexpr macro preproc_session cy86_backend
FRONTEND_OBJ_BASENAMES_cppgm++ := pp_tokenizer posttoken ctrlexpr macro \
	preproc_session pa10_ast pa10_declarator_shape pa10_parser_support pa10_renderer \
	pa11_semantic pa11_semantic_core pa11_semantic_types pa12_semantic \
	pa12_semantic_calls pa12_semantic_resolution pa12_semantic_facts abi_mangle lowir_model pa15_lowering pa15_lowering_flow
FRONTEND_OBJ_BASENAMES_lowiropt :=
FRONTEND_OBJ_BASENAMES_lowir2cy86 := lowir2cy86_backend lowir_model lowir_text_adapter
FRONTEND_OBJ_BASENAMES_lowir2native :=
