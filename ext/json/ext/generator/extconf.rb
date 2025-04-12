require 'mkmf'

if RUBY_ENGINE == 'truffleruby'
  # The pure-Ruby generator is faster on TruffleRuby, so skip compiling the generator extension
  File.write('Makefile', dummy_makefile("").join)
else
  append_cflags("-std=c99")
  $defs << "-DJSON_GENERATOR"
  
  # Add Ryu implementation files
  $srcs = %w[generator.c ryu.c ryu_platform.c]
  
  create_makefile 'json/ext/generator'
end
