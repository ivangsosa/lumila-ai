EXTRA_DIST = lumila-ai.desktop.in

datadir = $(datadir)/geany-plugins/lumila-ai
data_DATA = lumila-ai.desktop

lumila-ai.desktop: lumila-ai.desktop.in
	$(AM_V_GEN)sed -e 's|@VERSION@|$(VERSION)|g' \
	                -e 's|@NAME@|Lumila AI|g' \
	                -e 's|@DESCRIPTION@|AI Assistant for Geany - Multi-provider chat|g' \
	                -e 's|@AUTHOR@|Iván Gabriel Sosa|g' $< > $@

CLEANFILES = lumila-ai.desktop
