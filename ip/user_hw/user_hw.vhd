library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity user_HW is
   port (
      clk         : in  std_logic;
      reset_n     : in  std_logic;
      chipselect  : in  std_logic;
      address     : in  std_logic_vector(2 downto 0);
      write       : in  std_logic;
      writedata   : in  std_logic_vector(31 downto 0);
      read        : in  std_logic;
      readdata    : out std_logic_vector(31 downto 0)
   );
end entity user_HW;


architecture behavioral of user_HW is

   type mem_array_t is array (0 to 99) of std_logic_vector(7 downto 0);
   signal mem : mem_array_t := (others => (others => '0'));

   signal addr_reg : unsigned(6 downto 0);  -- 0..99
   signal data_reg : std_logic_vector(7 downto 0);
   signal ctrl_reg : std_logic_vector(1 downto 0);
   signal len_reg  : unsigned(6 downto 0);

   signal raw_byte       : std_logic_vector(7 downto 0);
   signal processed_byte : std_logic_vector(7 downto 0);

begin

   -- Leitura combinacional do array
   raw_byte <= mem(to_integer(addr_reg)) when addr_reg < 100
               else (others => '0');

   -- Processamento: char + 1, 0xF7 -> 0x00
   processed_byte <= x"00" when raw_byte = x"F7"
                     else std_logic_vector(unsigned(raw_byte) + 1);

   process (clk, reset_n)
   begin
      if reset_n = '0' then
         addr_reg <= (others => '0');
         data_reg <= (others => '0');
         ctrl_reg <= (others => '0');
         len_reg  <= (others => '0');
         mem      <= (others => (others => '0'));

      elsif rising_edge(clk) then

         -- Escrita nos registros pelo NIOS
         if chipselect = '1' and write = '1' then
            case address is
               when "000" =>
                  addr_reg <= unsigned(writedata(6 downto 0));
               when "001" =>
                  data_reg <= writedata(7 downto 0);
               when "010" =>
                  ctrl_reg <= writedata(1 downto 0);
               when "011" =>
                  len_reg <= unsigned(writedata(6 downto 0));
               when others =>
                  null;
            end case;
         end if;

         -- ctrl_reg(0) = 1 -> grava data_reg no array na posicao addr_reg
         if ctrl_reg(0) = '1' then
            if addr_reg < 100 then
               mem(to_integer(addr_reg)) <= data_reg;
            end if;
            ctrl_reg(0) <= '0';
         end if;

         -- ctrl_reg(1) = 1 -> le do array (processado) para data_reg
         if ctrl_reg(1) = '1' then
            data_reg    <= processed_byte;
            ctrl_reg(1) <= '0';
         end if;

      end if;
   end process;

   -- Leitura pelo NIOS
   readdata <=
      std_logic_vector(resize(addr_reg, 32))              when address = "000" else
      std_logic_vector(resize(unsigned(data_reg), 32))     when address = "001" else
      std_logic_vector(resize(unsigned(ctrl_reg), 32))     when address = "010" else
      std_logic_vector(resize(len_reg, 32))                when address = "011" else
      (others => '0');

end architecture behavioral;
