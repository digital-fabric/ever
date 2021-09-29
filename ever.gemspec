require_relative './lib/ever/version'

Gem::Specification.new do |s|
  s.name        = 'ever'
  s.version     = Ever::VERSION
  s.licenses    = ['MIT']
  s.summary     = 'Callback-less event reactor for Ruby'
  s.author      = 'Sharon Rosner'
  s.email       = 'sharon@noteflakes.com'
  s.files       = `git ls-files`.split
  s.homepage    = 'https://digital-fabric.github.io/ever'
  s.metadata    = {
    "source_code_uri" => "https://github.com/digital-fabric/ever",
    "homepage_uri" => "https://github.com/digital-fabric/ever",
    "changelog_uri" => "https://github.com/digital-fabric/ever/blob/master/CHANGELOG.md"
  }
  s.rdoc_options = ["--title", "ever", "--main", "README.md"]
  s.extra_rdoc_files = ["README.md"]
  s.extensions = ["ext/ever/extconf.rb"]
  s.require_paths = ["lib"]
  s.required_ruby_version = '>= 2.6'

  s.add_development_dependency  'rake-compiler',        '1.1.1'
  s.add_development_dependency  'minitest',             '5.14.4'
  s.add_development_dependency  'http_parser.rb',       '0.7.0'
  s.add_development_dependency  'nio4r',                '2.5.8'
end
