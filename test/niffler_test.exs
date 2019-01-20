defmodule NifflerTest do
  use ExUnit.Case
  doctest Niffler

  test "greets the world" do
    assert Niffler.hello() == :world
  end
end
