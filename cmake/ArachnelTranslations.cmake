function(arachnel_setup_translations target)
    find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

    # Every translations/arachnel_<lang>.ts is built and shown in the language lists; a language
    # added through Weblate needs no change here.
    file(GLOB _arachnel_ts_files CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/translations/arachnel_*.ts")
    list(SORT _arachnel_ts_files)

    qt_add_translations(${target}
        TS_FILES ${_arachnel_ts_files}
        RESOURCE_PREFIX /i18n
        LUPDATE_OPTIONS -no-obsolete
    )
endfunction()
