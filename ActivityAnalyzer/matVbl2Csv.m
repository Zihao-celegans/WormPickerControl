%matVbl2Csv.m
%
% Convert SessionData mat files to pseudo new-style analysis CSV file
clear; clc;
pdata = 'E:\RNAi_Project';
pout = fullfile(pdata,'_Analysis (Matlab)');

if ~isfolder(pout); mkdir(pout); end


% Cycle through the session data files

[d,N] = verify_dirlist(pdata,0,'.mat','Activity');

for n = 1:N
   
    % Load the  raw sum diff after data
    clear SessionData
    load(d(n).fullname,'SessionData')
    stim_act = nanmean(SessionData.raw_sum_diff_aft,1);
    
    
    % Change format of output file name
    outname = d(n).name;
    idx = min(strfind(outname,' '));
    outname(idx) = '_';
    
    % Save the result to an output file
    fout = fullfile(pout,strrep(outname,'.mat','.csv'));
    csvwrite(fout,stim_act)
    
end