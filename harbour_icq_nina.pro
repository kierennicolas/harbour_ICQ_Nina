# harbour_icq_nina: an ICQ-first OSCAR client for Sailfish OS, for the NINA
# (nina.chat) revival service.
#
# CONFIG+=sailfishapp_build (used by the RPM spec) builds only what ships:
# liboscar (installed privately) and the app.
TEMPLATE = subdirs

SUBDIRS = liboscar

# The Sailfish app needs libsailfishapp, i.e. the Sailfish SDK.
sailfishapp_build|packagesExist(sailfishapp) {
    SUBDIRS += app
    app.depends = liboscar
}

!sailfishapp_build {
    SUBDIRS += oscarprobe coretests
    oscarprobe.subdir = tools/oscarprobe
    oscarprobe.depends = liboscar
    # Desktop tests for the app core.
    coretests.subdir = tests/core
    coretests.depends = liboscar
}
