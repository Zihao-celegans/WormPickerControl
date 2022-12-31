clear;
close all;
clc;

%% Pool analysis script over different chromosomes
C_I_III_txt = {'JK2810XLX811_screen_F2_merge.txt'};
C_II_txt = {...
    'postprocess_VC170XLX811_screen_F2_test01_done_at_2022-03-23.txt';...
    'postprocess_VC170XLX811_screen_F2_test03_done_at_2022-03-23.txt'...
};
C_IV_V_txt = {...
    'postprocess_JK2958XLX811_screen_F2_test02_done_at_2022-03-22.txt';...
    'JK2958XLX811_screen_F2_merge.txt'...
};
C_III_V_txt = {...
    'postprocess_CGC34XLX811_screen_F2_test04_done_at_2022-03-22.txt';...
    'postprocess_CGC34XLX811_screen_F2_test05_done_at_2022-03-22.txt'...
};
C_X_txt = {...
    'VQ1155XLX811_F1_X_N2_screen_F2_clear_title.txt'; ...
    'VQ1155XLX811_F1_X_N2_screen_F2_test02_2022-03-10 _clear_title.txt'...
};

%% Autosome link theory
link_p = 1;
unlink_p = 0.75;
%% Chr I and III
[a1, a2] = reportRFPgvNonGFP(C_I_III_txt,... %tFile, 
    ["unknown", "herm", "male"], ... %sex
    ["unknown", "normal", "dpy"],... %morpho
    ["Adult", "L4"]); %stage
C_I_III_pattern = [a1, a2];
norm_C_I_III_pattern = C_I_III_pattern./sum(C_I_III_pattern);

% standard deviation
std_C_I_III = sqrt(sum(C_I_III_pattern)*unlink_p*(1-unlink_p));
std_norm_C_I_III = std_C_I_III/sum(C_I_III_pattern);

% linkage index 
idx_C_I_III = reportCosSim(link_p, unlink_p, C_I_III_pattern);

% linkage index variation with one std
up_idx_C_I_III = reportCosSim(link_p, unlink_p, [a1 + min(std_C_I_III, a2), a2 - min(std_C_I_III, a2)]);
low_idx_C_I_III = reportCosSim(link_p, unlink_p, [a1 - min(std_C_I_III, a1), a2 + min(std_C_I_III, a1)]);

%% Chr II
[b1, b2] = reportRFPgvNonGFP(C_II_txt,... %tFile, 
    ["unknown", "herm", "male"], ... %sex
    ["unknown", "normal", "dpy"],... %morpho
    ["Adult", "L4"]); %stage
C_II_pattern = [b1, b2];
norm_C_II_pattern = C_II_pattern./sum(C_II_pattern);

% standard deviation
std_C_II = sqrt(unlink_p*(1-unlink_p)*sum(C_II_pattern));
std_norm_C_II = std_C_II/sum(C_II_pattern);

% linkage index
idx_C_II = reportCosSim(link_p, unlink_p, C_II_pattern);

% linkage index variation with one std
up_idx_C_II = reportCosSim(link_p, unlink_p, [b1 + min(std_C_II, b2), b2 - min(std_C_II, b2)]);
low_idx_C_II = reportCosSim(link_p, unlink_p, [b1 - min(std_C_II, b1), b2 + min(std_C_II, b1)]);

%% Chr IV and V
[c1, c2] = reportRFPgvNonGFP(C_IV_V_txt,... %tFile, 
    ["unknown", "herm", "male"], ... %sex
    ["unknown", "normal", "dpy"],... %morpho
    ["Adult", "L4"]); %stage
C_IV_V_pattern = [c1, c2];
norm_C_IV_V_pattern = C_IV_V_pattern./sum(C_IV_V_pattern);

% standard deviation
std_C_IV_V = sqrt(unlink_p*(1-unlink_p)*sum(C_IV_V_pattern));
std_norm_C_IV_V = std_C_IV_V/sum(C_IV_V_pattern);

% linkage index
idx_C_IV_V = reportCosSim(link_p, unlink_p, C_IV_V_pattern);

% linkage index variation with one std
up_idx_C_IV_V = reportCosSim(link_p, unlink_p, [c1 + min(std_C_IV_V, c2), c2 - min(std_C_IV_V, c2)]);
low_idx_C_IV_V = reportCosSim(link_p, unlink_p, [c1 - min(std_C_IV_V, c1), c2 + min(std_C_IV_V, c1)]);

%% Chr III and V
[d1, d2] = reportRFPgvNonGFP(C_III_V_txt,... %tFile, 
    ["unknown", "herm", "male"], ... %sex
    ["unknown", "normal", "dpy"],... %morpho
    ["Adult", "L4"]); %stage
C_III_V_pattern = [d1, d2];
norm_C_III_V_pattern = C_III_V_pattern./sum(C_III_V_pattern);

% standard deviation
std_C_III_V = sqrt(unlink_p*(1-unlink_p)*sum(C_III_V_pattern));
std_norm_C_III_V = std_C_III_V/sum(C_III_V_pattern);

% linkage index
idx_C_III_V = reportCosSim(link_p, unlink_p, C_III_V_pattern);

% linkage index variation with one std
up_idx_C_III_V = reportCosSim(link_p, unlink_p, [d1 + min(std_C_III_V, d2), d2 - min(std_C_III_V, d2)]);
low_idx_C_III_V = reportCosSim(link_p, unlink_p, [d1 - min(std_C_III_V, d1), d2 + min(std_C_III_V, d1)]);


%% Chr X
unlink_X_p = 0.5;
link_X_p = 0;

[e1, e2, e3, e4] = reportRFPgvSexGFP(C_X_txt,... tFiles
    ["unknown", "normal", "dpy"],... %morpho
    ["Adult", "L4"]); %stage
C_X_herm_pattern = [e1, e2];
C_X_male_pattern = [e3, e4];
norm_C_X_herm_pattern = C_X_herm_pattern./sum(C_X_herm_pattern);
std_C_X_herm = sqrt(unlink_X_p*(1-unlink_X_p)/sum(C_X_herm_pattern));

% linkage index
idx_C_X_herm = reportCosSim(link_X_p, unlink_X_p, C_X_herm_pattern);

norm_C_X_male_pattern = C_X_male_pattern./sum(C_X_male_pattern);
std_C_X_male = sqrt(unlink_X_p*(1-unlink_X_p)*sum(C_X_male_pattern));
std_norm_C_X_male = std_C_X_male/sum(C_X_male_pattern);

% linkage index
idx_C_X_male = reportCosSim(link_X_p, unlink_X_p, C_X_male_pattern);


% linkage index variation with one std
up_idx_C_X_male = reportCosSim(link_X_p, unlink_X_p, [e3 - min(std_C_X_male, e3), e4 + min(std_C_X_male, e3)]);
low_idx_C_X_male = reportCosSim(link_X_p, unlink_X_p, [e3 +  min(std_C_X_male, e4), e4 - min(std_C_X_male, e4)]);

%% Plot bar graph

unlink_pat = [0.75, 0.25]; 
link_pat = [1, 0];

pool_data = [unlink_pat; link_pat; norm_C_I_III_pattern; norm_C_II_pattern; norm_C_III_V_pattern; norm_C_IV_V_pattern];

sz = size(pool_data);
kk = 1:sz(1);
%err = 0.1* ones(sz);

err = [0, 0 ; 0, 0; std_norm_C_I_III, std_norm_C_I_III; std_norm_C_II, std_norm_C_II; std_norm_C_III_V, std_norm_C_III_V; std_norm_C_IV_V, std_norm_C_IV_V];

figure(1);

hold on;
ax = bar(kk, pool_data, 'LineWidth',0.5);

ngroups = sz(1);
nbars = sz(2);

err_bar_k = 0.2;

% Calculating the width for each bar group
groupwidth = min(0.8, nbars/(nbars + 1.5));
for ee3 = 1:nbars
    x = (1:ngroups) - groupwidth/2 + (2*ee3-1) * groupwidth / (2*nbars);
    errorbar(x, pool_data(:,ee3), err(:,ee3), '.', 'Color', [err_bar_k err_bar_k err_bar_k], 'LineWidth', 1);
end


h_N = 1.05;
h_LI = 1;

% Put on sample number
text(3, h_N, strcat('N = ', num2str(sum(C_I_III_pattern))), 'FontSize',15)
text(4, h_N, strcat('N = ', num2str(sum(C_II_pattern))), 'FontSize',15)
text(5, h_N, strcat('N = ', num2str(sum(C_III_V_pattern))), 'FontSize',15)
text(6, h_N, strcat('N = ', num2str(sum(C_IV_V_pattern))), 'FontSize',15)


% Put on linkage index
% text(3, 1, strcat('Linkage Idx = ', num2str(sum(idx_C_I_III))), 'FontSize',15)
% text(4, 1, strcat('Linkage Idx = ', num2str(sum(idx_C_II))), 'FontSize',15)
% text(5, 1, strcat('Linkage Idx = ', num2str(sum(idx_C_III_V))), 'FontSize',15)
% text(6, 1, strcat('Linkage Idx = ', num2str(sum(idx_C_IV_V))), 'FontSize',15)


hold off

xticks(1:1:6)

xticklabels({'Autosome-unlink Pattern in Theory', 'Autosome-link Pattern in Theory', ...
    'Observed I/III-link Pattern', 'Observed II-link Pattern', 'Observed III/V-link Pattern', 'Observed IV/V link Pattern'})
a1 = get(gca,'XTickLabel');
set(gca,'XTickLabel',a1,'fontsize',15)
xtickangle(30)
legend('RFP', 'Non-RFP')
% legend('non-Red Male', 'Red Male', 'non-Red Male','Red Male', 'FontSize', 12, 'Location', 'northwest')
edge_k = 128/255;
ax(1).FaceColor = '#C30017'; ax(1).FaceAlpha = 0.3; ax(1).EdgeColor = [edge_k edge_k edge_k];
ax(2).FaceColor = '#CCCCCC'; ax(2).EdgeColor = '#808080';ax(2).EdgeColor = [edge_k edge_k edge_k];

%xlabel('Stage', 'FontSize', 15)
ylabel('Count Percentage given Non-GFP', 'FontSize', 15)
ylim([0 1.18])

%title('vsIs33 [dop-3::RFP] Linkage to Autosomes', 'FontSize', 20)


pbaspect([1 1/2 1]);

ax = gca;
set(ax,'TickLength',[0.01 0.02]);
set(ax.XAxis,'TickDir', 'out');
set(ax.YAxis,'TickDir', 'out');
set(gca,'box','off')

%% Plot X-link graph
unlink_X_herm_pat = [0.5, 0.5];
unlink_X_male_pat = [0.5, 0.5];

link_X_herm_pat = [1, 0];
link_X_male_pat = [0, 1];

%pool_X_data = [unlink_X_herm_pat; unlink_X_male_pat; link_X_herm_pat; link_X_male_pat; norm_C_X_herm_pattern; norm_C_X_male_pattern];

pool_X_data = [unlink_X_male_pat; link_X_male_pat; norm_C_X_male_pattern];

sz = size(pool_X_data);


kk = 1:sz(1);

%err = [0, 0 ; 0, 0; 0, 0 ; 0, 0; std_C_X_herm, std_C_X_herm; std_norm_C_X_male, std_norm_C_X_male];
err = [0, 0 ; 0, 0; std_norm_C_X_male, std_norm_C_X_male];

figure(2);
ax = bar(kk, pool_X_data, 'LineWidth',0.5);

hold on;
ngroups = sz(1);
nbars = sz(2);
% Calculating the width for each bar group
groupwidth = min(0.8, nbars/(nbars + 1.5));
for ee3 = 1:nbars
    x = (1:ngroups) - groupwidth/2 + (2*ee3-1) * groupwidth / (2*nbars);
    errorbar(x, pool_X_data(:,ee3), err(:,ee3), '.', 'Color', [err_bar_k err_bar_k err_bar_k], 'LineWidth', 1);
end

% Put on sample number
%text(5, 1, strcat('N = ', num2str(sum(C_X_herm_pattern))), 'FontSize',15)
text(3, h_N, strcat('N = ', num2str(sum(C_X_male_pattern))), 'FontSize',15)

% Put on linkage index

%text(5, 0.9, strcat('Linkage Idx = ', num2str(sum(idx_C_X_herm))), 'FontSize',15)
%text(3, 0.9, strcat('Linkage Idx = ', num2str(sum(idx_C_X_male))), 'FontSize',15)

hold off


% xticklabels({'Unlink X Herm Pattern in Theory','Unlink X Male Pattern in Theory',...
%     'Link X Herm Pattern in Theory','Link X Male Pattern in Theory', ...
%     'Observed X Link Herm Pattern', 'Observed X Link Male Pattern'})

% xticklabels({'X-link Pattern in Theory',...
%     'X-unlink Pattern in Theory', ...
%     'Observed X-link Pattern'})

% a1 = get(gca,'XTickLabel');
% set(gca,'XTickLabel',a1,'fontsize',15)
% xtickangle(30)
% legend('RFP', 'Non-RFP')
% legend('non-Red Male', 'Red Male', 'non-Red Male','Red Male', 'FontSize', 12, 'Location', 'northwest')


ax(1).FaceColor = '#C30017'; ax(1).FaceAlpha = 0.3; ax(1).EdgeColor = [edge_k edge_k edge_k];
ax(2).FaceColor = '#CCCCCC'; ax(2).EdgeColor = '#808080';ax(2).EdgeColor = [edge_k edge_k edge_k];

%xlabel('Stage', 'FontSize', 15)
% ylabel('Count Percentage given Male', 'FontSize', 15)
ylim([0 1.18])

%title('vsIs33 [dop-3::RFP] Linkage to X chromosome', 'FontSize', 20)

pbaspect([1 1 1]);

ax = gca;
set(ax,'TickLength',[0.01 0.02]);
set(ax.XAxis,'TickDir', 'out');
set(ax.YAxis,'TickDir', 'out');
set(gca,'box','off')

%% Plot linkage index over chromosomes
link_idices = [idx_C_I_III, idx_C_II, idx_C_III_V, idx_C_IV_V, idx_C_X_male];
figure(3);
ax = bar(link_idices, 'LineWidth',0.5);


ax.EdgeColor = [edge_k edge_k edge_k];
ax.FaceColor = '#2680FF';ax.FaceAlpha = 0.4;
hold on;

pos_err = [up_idx_C_I_III - idx_C_I_III, up_idx_C_II - idx_C_II, up_idx_C_III_V - idx_C_III_V, up_idx_C_IV_V - idx_C_IV_V, up_idx_C_X_male - idx_C_X_male];
neg_err = [idx_C_I_III - low_idx_C_I_III, idx_C_II - low_idx_C_II, idx_C_III_V - low_idx_C_III_V, idx_C_IV_V - low_idx_C_IV_V, idx_C_X_male - low_idx_C_X_male];

errorbar(1:size(link_idices, 2),link_idices,neg_err, pos_err, '.', 'Color', [err_bar_k err_bar_k err_bar_k], 'LineWidth', 1)
ylim([-0.6 1.05])


%xticklabels({'Chr I/III', 'Chr II', 'Chr III/V', 'Chr IV/V',' Chr X'})
a = get(gca,'XTickLabel');
set(gca,'XTickLabel',a,'fontsize',12)
% ylabel('Linkage Index', 'FontSize', 18)
hold off;

% title('vsIs33 [dop-3::RFP] Linkage over Chromosomes', 'FontSize', 20)

ax = gca;
set(ax,'TickLength',[0.02 0.02]);
set(ax.XAxis,'TickDir', 'out');
set(ax.YAxis,'TickDir', 'out');
set(gca,'box','off')



