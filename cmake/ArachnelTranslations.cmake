function(arachnel_setup_translations target)
    find_package(Qt6 REQUIRED COMPONENTS LinguistTools)
    if(TARGET Qt6::lrelease)
        get_target_property(_lrelease_exe Qt6::lrelease IMPORTED_LOCATION)
    endif()
    if(NOT _lrelease_exe)
        find_program(_lrelease_exe NAMES lrelease)
    endif()
    if(NOT _lrelease_exe)
        message(FATAL_ERROR "lrelease not found (Qt6 LinguistTools)")
    endif()

    set(_arachnel_ts_files
        "${CMAKE_CURRENT_SOURCE_DIR}/translations/arachnel_en.ts"
        "${CMAKE_CURRENT_SOURCE_DIR}/translations/arachnel_ru.ts"
    )

    set(_ids_dir "${CMAKE_CURRENT_SOURCE_DIR}/translations/generated")
    set(_ids_ts "${_ids_dir}/arachnel_ids.ts")
    set(_ids_qm "${_ids_dir}/arachnel_ids.qm")
    set(_ids_ru_qm "${CMAKE_CURRENT_BINARY_DIR}/arachnel_ids_ru.qm")
    set(_ru_ts "${CMAKE_CURRENT_SOURCE_DIR}/translations/arachnel_ru.ts")
    file(MAKE_DIRECTORY "${_ids_dir}")

    if(NOT EXISTS "${_ids_ts}")
        message(FATAL_ERROR
            "Missing ${_ids_ts}. Regenerate after editing id-based strings in arachnel_en.ts:\n"
            "  python tools/prepare_id_translations.py translations/generated/arachnel_ids.ts")
    endif()

    add_custom_command(
        OUTPUT "${_ids_qm}"
        COMMAND "${_lrelease_exe}" -idbased "${_ids_ts}" -qm "${_ids_qm}"
        DEPENDS "${_ids_ts}"
        COMMENT "Generating arachnel_ids.qm"
    )

    # Russian qsTrId catalog — same id messages as arachnel_ru.ts, but released with -idbased.
    add_custom_command(
        OUTPUT "${_ids_ru_qm}"
        COMMAND "${_lrelease_exe}" -idbased "${_ru_ts}" -qm "${_ids_ru_qm}"
        DEPENDS "${_ru_ts}"
        COMMENT "Generating arachnel_ids_ru.qm"
    )

    set_source_files_properties("${_ids_qm}" PROPERTIES
        GENERATED TRUE
        QT_RESOURCE_ALIAS "arachnel_ids.qm"
    )
    set_source_files_properties("${_ids_ru_qm}" PROPERTIES
        GENERATED TRUE
        QT_RESOURCE_ALIAS "arachnel_ids_ru.qm"
    )

    qt_add_translations(${target}
        TS_FILES ${_arachnel_ts_files}
        RESOURCE_PREFIX /i18n
        LUPDATE_OPTIONS -no-obsolete
    )

    # qsTrId() catalogs — not managed by lupdate (would wipe generated English .ts).
    qt_add_resources(${target} "arachnel_id_translations"
        PREFIX "/i18n"
        FILES
            "${_ids_qm}"
            "${_ids_ru_qm}"
    )

    add_custom_target(arachnel_ids_qm DEPENDS "${_ids_qm}" "${_ids_ru_qm}")
    add_dependencies(${target} arachnel_ids_qm)
endfunction()
