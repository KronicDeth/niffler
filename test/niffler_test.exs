defmodule NifflerTest do
  use ExUnit.Case

  alias Niffler.Treasure

  describe "burrow/0" do
    test "finds treasure" do
      assert {:ok, [%Treasure{name: "vial", value: 0}]} = Niffler.burrow()
    end
  end
end
