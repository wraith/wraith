#define NEXT_ARG	{ curr_arg++; argc--; }
static int tcl_compress_file STDVAR
{
  int mode_num = compress_level, result, curr_arg = 1;
  char *fn_src = NULL, *fn_target = NULL;
    BADARGS (2, 5, " ?options...? src-file ?target-file?");
  while ((argc > 1) && ((argv[curr_arg])[0] == '-'))
    {
      if (!strcmp (argv[curr_arg], "-level"))
	{
	  argc--;
	  if (argc <= 1)
	    {
	      Tcl_AppendResult (irp, "option `-level' needs parameter", NULL);
	      return TCL_ERROR;
	    }
	  curr_arg++;
	  mode_num = atoi (argv[curr_arg]);
	}
      else
	{
	  Tcl_AppendResult (irp, "unknown option `", argv[curr_arg], "'",
			    NULL);
	  return TCL_ERROR;
	}
      NEXT_ARG;
    }
  if (argc <= 1)
    {
      Tcl_AppendResult (irp, "expecting src-filename as parameter", NULL);
      return TCL_ERROR;
    }
  fn_src = argv[curr_arg];
  NEXT_ARG;
  if (argc > 1)
    {
      fn_target = argv[curr_arg];
      NEXT_ARG;
    }
  if (argc > 1)
    {
      Tcl_AppendResult (irp, "trailing, unexpected parameter to command",
			NULL);
      return TCL_ERROR;
    }
  if (fn_target)
    result = compress_to_file (fn_src, fn_target, mode_num);
  else
    result = compress_file (fn_src, mode_num);
  if (result)
    Tcl_AppendResult (irp, "1", NULL);
  else
    Tcl_AppendResult (irp, "0", NULL);
  return TCL_OK;
}
static int tcl_uncompress_file STDVAR
{
  int result;
    BADARGS (2, 3, " src-file ?target-file?");
  if (argc == 2)
    result = uncompress_file (argv[1]);
  else
      result = uncompress_to_file (argv[1], argv[2]);
  if (result)
      Tcl_AppendResult (irp, "1", NULL);
  else
      Tcl_AppendResult (irp, "0", NULL);
    return TCL_OK;
}
static int tcl_iscompressed STDVAR
{
  int result;
    BADARGS (2, 2, " compressed-file");
    result = is_compressedfile (argv[1]);
  if (result == COMPF_UNCOMPRESSED)
    Tcl_AppendResult (irp, "0", NULL);
  else if (result == COMPF_COMPRESSED)
    Tcl_AppendResult (irp, "1", NULL);
  else
      Tcl_AppendResult (irp, "2", NULL);
    return TCL_OK;
}
static tcl_cmds my_tcl_cmds[] =
  { {"compressfile", tcl_compress_file}, {"uncompressfile",
					  tcl_uncompress_file},
  {"iscompressed", tcl_iscompressed}, {NULL, NULL} };
