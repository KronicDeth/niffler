defmodule NifflerTest do
  use ExUnit.Case

  alias Niffler.Treasure

  @time_to_find 5_000

  describe "burrow/0" do
    test "with timeout less than time-to-find returns {:error, :timeout}" do
      timeout = 100

      assert timeout < @time_to_find
      assert {:error, :timeout} = Niffler.burrow(timeout)
    end

    test "with elapsed greater than time-to-find returns {:ok, treasures}" do
      elapsed = 10_000

      assert @time_to_find < elapsed
      assert {:ok, [%Treasure{name: "vial", value: 0}]} = Niffler.burrow(:infinity, elapsed)
    end

    test "ties between elapsed time and timeout goes to elapsed time" do
      time = @time_to_find

      assert {:ok, [%Treasure{name: "vial", value: 0}]} = Niffler.burrow(time, time)
    end

    test "finds treasure" do
      assert {:ok, [%Treasure{name: "vial", value: 0}]} = Niffler.burrow()
    end
  end
end
