function img = contours2img(contours)
%UNTITLED5 Summary of this function goes here
%   Detailed explanation goes here

img = zeros(480,640);
for i=1:size(contours)
    [N_pts, ~] = size(contours{i});
    for j = 1:N_pts
        img(contours{i}(j,1), contours{i}(j,2)) = 255;
    end
end

end

