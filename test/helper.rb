# frozen_string_literal: true

require 'bundler/setup'

require 'fileutils'
require 'minitest/autorun'

module Minitest::Assertions
  def assert_in_range exp_range, act
    msg = message(msg) { "Expected #{mu_pp(act)} to be in range #{mu_pp(exp_range)}" }
    assert exp_range.include?(act), msg
  end
end
