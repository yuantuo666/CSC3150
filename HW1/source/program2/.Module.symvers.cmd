cmd_/home/csc3150/csc3150/source/program2/Module.symvers := sed 's/ko$$/o/' /home/csc3150/csc3150/source/program2/modules.order | scripts/mod/modpost -m -a   -o /home/csc3150/csc3150/source/program2/Module.symvers -e -i Module.symvers   -T -