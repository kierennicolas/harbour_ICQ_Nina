# harbour_icq_nina: an ICQ-first OSCAR client for Sailfish OS, for the NINA
# (nina.chat) revival service.
TEMPLATE = subdirs

SUBDIRS = liboscar oscarprobe
oscarprobe.subdir = tools/oscarprobe
oscarprobe.depends = liboscar
