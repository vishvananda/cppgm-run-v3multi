# Per-tool implementation source lists for the compiler.
#
# Add dev/src/foo.cpp to the tools that use it by adding `foo` below. For
# subdirectories, use the path without `.cpp`, such as `parser/foo`.

FRONTEND_SOURCE_SET_TARGETS := abimangle pptoken posttoken ctrlexpr macro preproc recog nsdecl nsinit cy86 cppgm++ lowiropt lowir2cy86 lowir2native

FRONTEND_OBJ_BASENAMES_abimangle :=
FRONTEND_OBJ_BASENAMES_pptoken := pp_tokenizer
FRONTEND_OBJ_BASENAMES_posttoken := pp_tokenizer posttoken
FRONTEND_OBJ_BASENAMES_ctrlexpr := pp_tokenizer posttoken ctrlexpr
FRONTEND_OBJ_BASENAMES_macro := pp_tokenizer posttoken macro
FRONTEND_OBJ_BASENAMES_preproc := pp_tokenizer posttoken ctrlexpr macro preproc_session
FRONTEND_OBJ_BASENAMES_recog := pp_tokenizer posttoken ctrlexpr macro preproc_session cpp_declaration_syntax pa6_recognizer pa6_parser pa6_parser_declarations pa6_parser_expressions
FRONTEND_OBJ_BASENAMES_nsdecl := pp_tokenizer posttoken ctrlexpr macro preproc_session cpp_declaration_syntax pa7_semantic
FRONTEND_OBJ_BASENAMES_nsinit := pp_tokenizer posttoken ctrlexpr macro preproc_session cpp_declaration_syntax pa8_semantic
FRONTEND_OBJ_BASENAMES_cy86 :=
FRONTEND_OBJ_BASENAMES_cppgm++ :=
FRONTEND_OBJ_BASENAMES_lowiropt :=
FRONTEND_OBJ_BASENAMES_lowir2cy86 :=
FRONTEND_OBJ_BASENAMES_lowir2native :=
