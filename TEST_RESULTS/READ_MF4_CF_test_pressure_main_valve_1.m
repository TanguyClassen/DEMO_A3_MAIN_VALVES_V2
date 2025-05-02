% data = mdfRead("CF_test_pressure_main_valve_1.mf4")

dataChanExact = mdfRead("CF_test_pressure_main_valve_1.mf4", Channel=["time", "Tank pressure ethanol", "Tank weight ethanol"])

Export = dataChanExact{1}

% Write the table to a CSV file

writetimetable(Export,'CF_test_pressure_main_valve_1_CSV.csv')