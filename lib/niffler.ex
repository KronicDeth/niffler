defmodule Niffler do
  @on_load {:init, 0}

  @doc """
  Burrow for treasure.
  """
  def burrow(timeout \\ :infinity, elapsed \\ 0)
      when (timeout == :infinity or (is_integer(timeout) and 0 < timeout)) and is_integer(elapsed) and
             elapsed >= 0 do
    :erlang.nif_error(:not_loaded)
  end

  @doc false
  def init do
    case load_nif() do
      :ok -> :ok
      _ -> raise "An error occurred when loading Niffler."
    end
  end

  defp load_nif do
    :niffler
    |> :code.priv_dir()
    |> :filename.join('niffler')
    |> :erlang.load_nif(0)
  end
end
