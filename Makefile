# This Makefile is for the convenience of the package developers,
# and is not meant for use by end-users.

reinstall: pyclean pipclean
#	pip install . --verbose
	pip install . --user --verbose --no-cache
matteo:
	rm -rf build
	pip uninstall opentims -y || true
	pip install . --verbose --no-cache 
cached:
	pip uninstall timsdata -y || pip uninstall opentims -y || true
	pip install . --verbose > log
rprep: rclean
	R CMD build R
rcheck: rprep
	R CMD check opentims_*.tar.gz
rinst: rprep
	R CMD INSTALL opentims_*.tar.gz
ipython:
	python -m IPython
clean: pyclean rclean hereclean
rclean:
	rm -rf opentims_*.tar.gz  opentims.Rcheck
pyclean:
	rm -rf build dist opentims.egg-info
hereclean:
	rm -f *.so a.out
pipclean:
	pip uninstall opentims -y || true
	pip uninstall opentims -y || true
