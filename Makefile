# This Makefile is for the convenience of the package developers,
# and is not meant for use by end-users.

reinstall: pyclean pipclean
	pip install . --user --verbose --no-cache
reinstall_ve: pyclean pipclean
	pip install . --verbose --no-cache 
rdoc:
	Rscript rdocs.R
rbuild:
	R CMD build opentimsr
rprep: rclean rdoc rclean rbuild

rcheck: rprep
	R CMD check opentimsr_*.tar.gz
rcheck_as_CRAN: rprep
	R CMD check --as-cran opentimsr_*.tar.gz
rfullinst: rprep
	R CMD INSTALL opentimsr_*.tar.gz
rinst: rclean rbuild
	R CMD INSTALL opentimsr_*.tar.gz
ipy:
	python -m IPython
clean: pyclean rclean hereclean
rclean:
	rm -rf opentimsr_*.tar.gz  opentimsr.Rcheck opentimsr/src/*.o opentimsr/src/*.so opentimsr/src/*.tmp
pyclean:
	rm -rf build dist opentimspy.egg-info *.whl
hereclean:
	rm -f *.so a.out
pipclean:
	pip uninstall opentimspy -y || true
	pip uninstall opentimspy -y || true
docs: clean_docs
	git branch gh-pages || true
	git checkout gh-pages || true
	pip install sphinx || true
	pip install recommonmark || true
	mkdir -p sphinx
	sphinx-quickstart sphinx --sep --project OpenTIMS --author Lacki_and_Startek -v 0.0.1 --ext-autodoc --ext-githubpages --extensions sphinx.ext.napoleon --extensions recommonmark --makefile -q --no-batchfile
	sphinx-apidoc -f -o sphinx/source opentimspy
	cd sphinx && make html
	cp -r sphinx/build/html docs
	touch docs/.nojekyll
update_docs:
	rm -rf docs
	mkdir -p docs
	cd sphinx && make html
	cp -r sphinx/build/html/* docs
show_docs:
	firefox docs/index.html
clean_docs:
	rm -rf sphinx
	rm -rf docs
