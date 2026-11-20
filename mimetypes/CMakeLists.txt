install(FILES
  all.desktop
  allfiles.desktop
  DESTINATION ${KDE3_MIME_INSTALL_DIR}/all)

add_subdirectory(application)
add_subdirectory(text)
add_subdirectory(audio)
add_subdirectory(video)
add_subdirectory(image)
add_subdirectory(inode)
add_subdirectory(message)
add_subdirectory(multipart)
add_subdirectory(model)
