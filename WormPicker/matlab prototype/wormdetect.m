path = 'C:\WormPicker\recordings\for_debugging\mov.avi';

thresh = 89;
minsize = 70;


mov = VideoReader(path);

flickering = nan(0,0);
maxframe = 100;

imgseq = nan(420, 560, 3,maxframe);

frame_num = 0;

while hasFrame(mov)
    frame_num = frame_num + 1;
    img = readFrame(mov);
    frame = rgb2gray(img);
    [h, w, x] = size(frame);

    background = imgaussfilt(frame, 21);
    background = background - 100;
    frame = frame - background;
    frame = imgaussfilt(frame, 2);
    frame = im2bw(frame, thresh/255);
    frame = not(frame);
    frame = bwareaopen(frame, minsize);

    figure(1)
    subplot(1,2,2), imagesc(frame)
    contours = bwboundaries(frame);
    warning('off','MATLAB:polyshape:repairedBySimplify');
    
    
    subplot(1,2,1), 
    image(img)    
    hold on
    for i=1:size(contours)
        [N_points, ~] = size(contours{i});
        if ( N_points > 3)
            [iskeep, a, p, l, w, x, y] = polyshape_filter(contours{i});
            if (~iskeep)
                contours{i} = [];
            else
                plot(contours{i}(:,2), contours{i}(:,1), 'color', 'y')
            end
        end
        
    end

    hold off
    disp(frame_num)
    
    contours(~cellfun('isempty',contours));
    contoursimg = contours2img(contours);
    A = getframe(figure(1));
    imgseq(:,:,:,frame_num) = A.cdata;
    
    if frame_num > maxframe
        break
    end
    
end