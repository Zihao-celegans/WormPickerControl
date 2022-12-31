function [RgvHermG, NRgvHermG, RgvMaleG, NRgvMaleG] = reportRFPgvSexGFP(tFiles, morph_wanted, stage_wanted)

%% Load data from txts
sz = size(tFiles);

M = [];
for i = 1:sz(1)
    data = readmatrix(tFiles{i});
    data = data(:,all(~isnan(data)));
    M = [M; data];
end

M = M(:,all(~isnan(M)));

%% Start data filtering
%% filter morphology
if (~ismember("unknown", morph_wanted))
    % Delete unknown
    fprintf('Delete unknown morphology \n'); 
    M = M(~(M(:, 4) == 0),:);
end

if (~ismember("normal", morph_wanted))
    % Delete normal
    fprintf('Delete normal \n'); 
    M = M(~(M(:, 4) == 1),:);
end

if (~ismember("dpy", morph_wanted))
    % Delete dumpy
    fprintf('Delete dumpy \n'); 
    M = M(~(M(:, 4) == 2),:);
end
%% filter stage
if (~ismember("unknown", stage_wanted))
    % Delete unknown
    fprintf('Delete unknown stage \n'); 
    M = M(~(M(:, 5) == 0),:);
end


if (~ismember("Adult", stage_wanted))
    % Delete unknown
    fprintf('Delete Adult \n'); 
    M = M(~(M(:, 5) == 1),:);
end

if (~ismember("L4", stage_wanted))
    % Delete unknown
    fprintf('Delete L4 \n'); 
    M = M(~(M(:, 5) == 2),:);
end

if (~ismember("L3", stage_wanted))
    % Delete unknown
    fprintf('Delete L3 \n'); 
    M = M(~(M(:, 5) == 3),:);
end

if (~ismember("L2", stage_wanted))
    % Delete unknown
    fprintf('Delete L2 \n'); 
    M = M(~(M(:, 5) == 4),:);
end

if (~ismember("L1", stage_wanted))
    % Delete unknown
    fprintf('Delete L1 \n'); 
    M = M(~(M(:, 5) == 5),:);
end

%%

RgvHermG = sum (M(:, 3) == 1 & M(:, 1) == 1 & M(:, 2) == 1);
NRgvHermG = sum (M(:, 3) == 2 & M(:, 1) == 1 & M(:, 2) == 1);
RgvMaleG = sum (M(:, 3) == 1 & M(:, 1) == 2 );
NRgvMaleG = sum (M(:, 3) == 2 & M(:, 1) == 2 );

end