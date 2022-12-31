function link_idx = reportCosSim(theory_link_p, theory_unlink_p, observation)
%% Vector representation for RFP
% pattern for link and unlink in theory
link = [theory_link_p, 1 - theory_link_p]';
unlink = [theory_unlink_p, 1 - theory_unlink_p]';

e_link = link/norm(link);
e_unlink = unlink/norm(unlink);

E = [e_link, e_unlink];

% observed rfp pattern given G
observation = observation';

% represent the observed pattern by link and unlink patterns
observation_E  = E\observation;

e_link_E = E\e_link;

e_unlink_E = E\e_unlink;

% cosine similarity
link_idx = max(min(dot(observation_E, e_link_E)/(norm(observation_E)*norm(e_link_E)),1),-1);
unlink_idx = max(min(dot(observation_E, e_unlink_E)/(norm(observation_E)*norm(e_unlink_E)),1),-1);
end