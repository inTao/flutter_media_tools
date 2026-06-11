Pod::Spec.new do |s|
  s.name             = 'flutter_media_tools'
  s.version          = '0.0.1'
  s.summary          = 'flutter media tools'
  s.description      = <<-DESC
flutter media tools
                       DESC
  s.homepage         = 'http://example.com'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Your Company' => 'email@example.com' }

  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'

  s.dependency 'FlutterMacOS'

  s.platform = :osx, '10.11'
  s.pod_target_xcconfig = { 'DEFINES_MODULE' => 'YES' }
  s.swift_version = '5.0'

  s.vendored_libraries = 'Libraries/libnative.dylib'
end
