% ----------------------------
% Parameters (easy to modify)
% ----------------------------

fileNames = {
    'CF_test_pressure_main_valve_1.mf4'
    'CF_test_pressure_main_valve_2.mf4'
    'CF_test_pressure_main_valve_3.mf4'
};

timeStepSeconds = 0.001; % <<<<<<<<<<<<<< Change precision here (e.g., 0.001 for 1 ms, 0.01 for 10 ms)

maxExcelRows = 1048575; % Excel .xlsx limit minus 1 row for headers

% ----------------------------
% Processing Loop
% ----------------------------

for i = 1:length(fileNames)
    
    % Read the data
    dataChanExact = mdfRead(fileNames{i}, 'Channel', ["time", "Tank pressure ethanol", "Tank weight ethanol"]);
    
    % Extract the two timetables
    tt1 = dataChanExact{1};
    tt2 = dataChanExact{2};
    
    % Synchronize the two timetables based on time
    mergedData = synchronize(tt1, tt2, 'union', 'linear');
    
    % Rename variables (optional but cleaner)
    mergedData.Properties.VariableNames = ["TankPressureEthanol", "TankWeightEthanol"];
    
    % ----------------------------
    % Downsample to reduce data
    % ----------------------------
    mergedData = retime(mergedData, 'regular', 'linear', 'TimeStep', seconds(timeStepSeconds));
    
    % Add Time as a regular column
    mergedData.Time = mergedData.Properties.RowTimes;
    mergedData = movevars(mergedData, 'Time', 'Before', 1);
    
    % Convert to table
    mergedTable = timetable2table(mergedData);
    
    % Get base file name
    [~, baseFileName, ~] = fileparts(fileNames{i});
    excelFileName = [baseFileName, '.xlsx'];
    
    % ----------------------------
    % Export to Excel, with splitting if needed
    % ----------------------------
    numRows = height(mergedTable);
    
    if numRows <= maxExcelRows
        % If fits, write normally
        writetable(mergedTable, excelFileName);
    else
        % If too big, split into multiple sheets
        numSheets = ceil(numRows / maxExcelRows);
        for s = 1:numSheets
            idxStart = (s-1)*maxExcelRows + 1;
            idxEnd = min(s*maxExcelRows, numRows);
            subTable = mergedTable(idxStart:idxEnd, :);
            
            writetable(subTable, excelFileName, 'Sheet', sprintf('Sheet%d', s));
        end
    end
    
    % Display progress
    fprintf('Exported %s successfully.\n', excelFileName);
    
end
