<?php

include_once(dirname(__FILE__) . '/provision_git_project.context.service.inc');

/**
 * Implements hook_drush_command
 */
function provision_git_project_drush_command(){
  $items = array();

  $items['git-project-create'] = array(
      'description' => 'Creates a project.',
      'arguments' => array(
        'project-name' => 'Simple name for the project',
        'base-domain'  => 'Base-domain the deployments should be based on',
        'makefile-url' => 'Raw url for the makefile on github',
        ),
      'options' => array(
        'profile' => array(
          'description' => 'Default install profile for deployments',
          ),
        'db_server' => array(
          'description' => 'Alias for the server that hosts the database to be used by deployments'
          ),
        'web_server' => array(
          'description' => 'Alias for the webserver that hosts the deployments'
          ),
        ),
          'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap at all.

    );

   $items['git-project-deployment-define'] = array(
    'description' => "Creates/updates a deployment definition.",
    'arguments' => array(
      'project-alias' => 'Alias a existing project project',
      'deployment-name' => 'Simple name for the deployment'
     ),
    'required-arguments' => TRUE,
    'options' => array(
      'branch' => array(
        'description' => 'Make the deployment track a branch (new instances of the deployment will use branch HEAD)',
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap at all.
  );

  $items['git-project-deploy'] = array(
    'description' => "Builds a platform from a git repo and installs or migrates a site to it",
    'arguments' => array(
      'project' => 'name of the project to be deployed or context starting with @',
      'deployment' => 'name of the deployment to perform or context starting with @',
     ),
    'options' => array(
      'tag' => array(
        'description' => 'git tag to use for deployment, optional if the deployment tracks a branch.',
      ),
      'sha' => array(
        'description' => 'git SHA to use for deployment, optional if the deployment tracks a branch.',
      ),
      'branch' => array(
        'description' => 'branch (head) to use for deployment, the deployment will "track" this branch. Subsequent invokcations of git-platform-deploy will not require you to reference this branch if all you want to do is to migrate to head of the branch',
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH, // No bootstrap at all.
  );


  return $items;
}

/**
 * Verify that both project and deployment exists as aliases
 */
function drush_provision_git_project_deploy_validate($project_alias = FALSE, $deployment_name = FALSE){
  if($project_alias === FALSE){
    return drush_set_error('MISSING_ARGUMENT', dt('Missing: project'));
  }

  if($deployment_name === FALSE){
    return drush_set_error('MISSING_ARGUMENT', dt('Missing: deployment'));
  }

  // attempt to load the project
  $project = d(_git_project_get_project_alias($project_alias));
  if(!$project){
    // d() has already printed an error messeage, so just return false
    return FALSE;
  }

 // attempt to load the project
  $deployment = d(_git_project_get_deployment_alias($project_alias, $project->project_name));

  if(!$deployment){
    // d() has already printed an error messeage, so just return false
    return FALSE;
  }

  //TODO halt if the user specifeid more than one rev spec
}

function drush_provision_git_project_deploy($project_name, $deployment_name){

  $project_alias = _git_project_get_project_alias($project_name);
  $project = d($project_alias);
  $deployment_alias = _git_project_get_deployment_alias($deployment_name, $project);
  $deployment = d($deployment_alias);

  // revision info
  $tag = $deployment_branch = drush_get_option('tag', FALSE);
  $sha = $deployment_branch = drush_get_option('sha', FALSE);
  $branch = $deployment_branch = drush_get_option('branch', FALSE);

  // TODO, store branch in context if specified

  // TODO move into utility method so that we can verify at project creation
  $url = parse_url($project->makefile_url);
  $params = array();
  if(isset($url['query'])){
    parse_str($url['query'], $params);
  }
  var_dump($url);
  var_dump($params);

  if($tag !== FALSE) {
    $rev = $tag;
  } else if($sha !== FALSE) {
    $rev = $sha;
  } else if($branch !== FALSE) {
    $rev = $branch;
  } else {
    $rev = $deployment->deployment_branch;
  }

  // path constsis of /<user/company>/<repository>/<branch/tag/sha>/<path>
  if(!preg_match('#^/(.*?)/(.*?)/(.*?)/(.*)#', $url['path'], $matches)){
    return drush_set_error("INVALID_URL", "could not parse makefile-url");
  }
  list($null, $gh_user, $repo, $null, $path) = $matches;

  // build up the url again
  $fetch_url = $url['scheme'] . '://' . $url['host'] . '/' . $gh_user . '/' . $repo . '/' . $rev . '/' . $path;
  $fetch_url .= '?' . $url['query'];

  // download makefile
  // TODO, split up into functions

  // TODO, make up clever platform name, based on timestamps and/or revisions
  $platform_name = str_replace('@', '', $deployment_alias . '_' . time());

  // TODO, get destination from somewhere
  $platform_destination = '/var/aegir/platforms/' . $platform_name;

  if(file_exists($platform_destination)){
    drush_set_error('PLATFORM_EXISTS', dt('Could not create platform-directory "!dir" as it already exists', array('!dir' => $platform_destination)));
    return FALSE;
  }

  // prepare revision info
  if($sha !== FALSE) {
      $repo_info['revision'] = $sha;
  } else if($branch !== FALSE){
      $repo_info['branch'] = $branch;
  } else  if($tag !== FALSE){
      $repo_info['tag'] = $tag;
  } else {
    // default to deployment branch
    $repo_info['branch'] = $deployment->deployment_branch;
  }

  // TODO use a temp file?
  $temp_repo = drush_tempdir();
  $makefile_content = file_get_contents($fetch_url);
  $makefile_path = $temp_repo . '/' . 'stub_makefile.make';
  file_put_contents($makefile_path, $makefile_content);

  // TODO, figure out how to handle this
  $project_name = 'sclerosis';

  if(!_git_project_rewrite_makefile($makefile_path, $project_name, $repo_info)){
    return FALSE;
  }

  drush_log(dt('Using @makefile to build platform', array('@platform' => $platform_name, '@makefile' => $makefile_path)), 'ok');

  //TODO we might have to bootstrap to a higher level?
  drush_backend_invoke_args('make', array($makefile_path, $platform_destination), array('root' => null, 'uri' => null), 'GET');


  if (drush_get_error()) {
    return drush_set_error("DRUSH_MAKE_FAILED", "Could not build platform from makefile");
  }

  drush_log(dt('Platform "!platform" prepared, installing it into Aegir', array('!platform' => $platform_name), 'ok'));
}


function drush_provision_git_project_deployment_define_validate($project_alias = FALSE, $deployment_name = FALSE){
  if($project_alias === FALSE){
    return drush_set_error('MISSING_ARGUMENT', dt('Missing argument: project-alias'));
  }

  if($deployment_name === FALSE || strlen(_git_project_alias_normalize($deployment_name)) == 0){
    return drush_set_error('MISSING_ARGUMENT', dt('Missing or invalid argument: deployment-name'));
  }

  // attempt to load the project
  $project = d(_git_project_get_project_alias($project_alias));

  if(!$project){
    // d() has already printed an error messeage, so just return false
    return FALSE;
  }
}

function drush_provision_git_project_deployment_define($project_alias, $deployment_name){
  $project_alias = _git_project_get_project_alias($project_alias);
  $project = d($project_alias);

  // make sure the alias confirms
  $deployment_alias = _git_project_get_deployment_alias($deployment_name, $project);

  $deployment_branch = drush_get_option('branch', 'master');

  // prepare arguments for provision-save
  $args = array($deployment_alias);
  $options = array( 'context_type'      => 'git_project_deployment',
                    'deployment_name'   => $deployment_name,
                    'deployment_branch' => $deployment_branch,
                    'status'            => 'defined',
                    'git_project'       => $project_alias,
                    );
  // save the context
  drush_backend_invoke_args('provision-save', $args, $options + array('root' => null, 'uri' => null), 'GET');

  // output result
  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", "Could not create deployment alias");
  }
  drush_log(dt('Stored the deployment "!deployment" under the context !alias',
    array( '!alias' => $deployment_alias,
           '!deployment' => $deployment_name,
      )
    ), 'ok');
  echo("\n" . _git_project_deployment_info($deployment_alias));
}

function drush_provision_git_project_create($project_name, $base_domain, $makefile_url){
  // prepare the remaining arguments ()
  $profile = drush_get_option('profile', 'default');
  $web_server = drush_get_option('web_server', '@server_master');
  $db_server = drush_get_option('db_server', '@server_localhost');

  // normalize project name
  $project_alias = _git_project_get_project_alias($project_name);

  // save the context
  $args = array($project_alias);
  $options = array( 'context_type'    => 'git_project',
                    'project_name'    => $project_name,
                    'makefile_url'    => $makefile_url,
                    'profile'         => $profile,
                    'domain_basename' => $base_domain,
                    'db_server'       => $db_server,
                    'web_server'      => $web_server,
                   );

  drush_backend_invoke_args('provision-save', $args, $options + array('root' => null, 'uri' => null), 'GET');

  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", "Could not create project alias");
  }
  drush_log(dt('Stored project "!project" under the context !alias',
    array( '!alias' => $project_alias,
           '!project' => $project_name,
      )
    ), 'ok');
  echo("\n" . _git_project_project_info($project_alias));
}


function _git_project_deployment_info($alias){
  $p = d($alias);

  $out['Name']             = $p->deployment_name;
  $out['Project']          = $p->git_project->project_name;
  $out['Branch']           = $p->deployment_branch;
  $out['Current Tag']      = $p->deployment_tag;
  $out['Current Revision'] = $p->deployment_revision;
  $out['Status']           = $p->status;
  $out['Site']             = $p->site;
  $out['Platform']         = $p->platform;


  return _git_project_print_array($out, 'Deployment info');
}


function _git_project_project_info($alias){
  $p = d($alias);

  $out['Project Name']    = $p->project_name;
  $out['Install Profile'] = $p->profile;
  $out['Domain Basename'] = $p->domain_basename;
  $out['DB Server']       = $p->db_server;
  $out['Web Server']      = $p->web_server;

  return _git_project_print_array($out, 'Project info');
}

function _git_project_print_array($array, $title = FALSE){
  $max_len = 0;
  foreach($array as $key => $value){
    $max_len = $max_len < strlen($key) ? strlen($key) : $max_len;
  }
  $whitespace_count = $max_len + 1;

  $out_string = '';

  // add title if we have one
  if($title !== FALSE){
    $out_string = $title . "\n" . str_repeat('=', strlen($title)) . "\n";
  }

  // add key/values with proper spacing
  foreach($array as $key => $value){
    $out_string .= $key . str_repeat(' ', $max_len - strlen($key))
                   . ' : ' . (strlen($value) > 0 ? $value : '<empty>') . "\n";
  }

  return($out_string);
}

// UTILITY FUNCTION

/**
 * @return drush alias for the project name including @
 */
function _git_project_get_project_alias($project_alias){
  return _git_project_get_alias($project_alias, 'git_project_');
}

/**
 * @return drush alias for the deployment name including @
 */
function _git_project_get_deployment_alias($deployment_alias, $project){
  return _git_project_get_alias($deployment_alias, 'git_project_deployment_' . _git_project_alias_normalize($project->project_name) . '_');
}

/**
 * @return normalized alias based on name. Returns $name if it starts with @
 */
function _git_project_get_alias($name, $prefix = ''){
  return (substr($name, 0, 1) == '@') ? $name : '@' . $prefix . _git_project_alias_normalize($name);
}

/**
 * converts the name to lowercase and strips all non-word chars except for .-
 */
function _git_project_alias_normalize($name){
  return strtolower(preg_replace("/[^\w\.-]/", "", $name));
}


/**
 * Performs a checkout of a project and installs it as a platform, optionally
 * migrating or installing a new site on it
 *
 * (ref https://github.com/mig5/aegir_cid/blob/master/fabfile.py)
 *
 * TODO:
 *  - switch to using proper aegir d() contexts
 */
function drush_provision_git_platform($project_name, $platform_name, $host, $repo) {
  // get paramteres
  $repo_info = array();
  $repo_info['url'] = $repo;

  $platform_destination = '/var/aegir/platforms/' . $platform_name;
  if(file_exists($platform_destination)){
    drush_set_error('PLATFORM_EXISTS', dt('Could not create platform-directory "@dir" as it already exists', array('@dir' => $platform_destination)));
    return FALSE;
  }

  if($revision = drush_get_option('checkout-revision')){
      $repo_info['revision'] = $revision;
  }

  if($branch = drush_get_option('checkout-branch')){
      $repo_info['branch'] = $branch;
  }

  if($branch = drush_get_option('checkout-tag')){
      $repo_info['tag'] = $branch;
  }

  drush_log(dt('Building the platform @platform', array('@platform' => $platform_name)), 'ok');

  // if a makefile is specified, do a temp checkout, get the file, modify it and feed it to drush make
  // TODO some caching should allow us to save a checkout
  if(!($makefile = drush_get_option('makefile'))){
    // plain checkout
    if(!drush_make_download_git($project_name, $repo_info, $platform_destination)){
      return FALSE;
    }

  }else{
    // get repo into temp location
    $temp_repo = drush_tempdir();
    if(!drush_make_download_git($project_name, $repo_info, $temp_repo)){
      return FALSE;
    }

    // find the make-file
    $makefile_path = $temp_repo . '/' . $makefile;
    if(!file_exists($makefile_path)){
      return drush_set_error('MAKEFILE_NOT_FOUND', dt('Could not find makefile "@file" in checkout', array('@file' => $makefile)));
    }

    if(!_git_project_rewrite_makefile($makefile_path, $project_name, $repo_info)){
      return FALSE;
    }

    drush_log(dt('Using @makefile to build platform', array('@platform' => $platform_name, '@makefile' => $makefile)), 'ok');

    provision_backend_invoke('self', 'make', array($makefile_path, $platform_destination));

    if (drush_get_error()) {
      return drush_set_error("DRUSH_MAKE_FAILED", "Could not build platform from makefile");
    }
  }

  drush_log(dt('Platform "!platform" prepared, installing it into Aegir', array('!platform' => $platform_name), 'ok'));

  // platform files are now in place
  // setup the aegir part
  $platform_alias = '@platform_' . $platform_name;

  // setup server aliases
  $webserver = '@' . (drush_get_option('webserver') ? drush_get_option('webserver') : 'server_master');
  $dbserver = '@' . (drush_get_option('dbserver') ? drush_get_option('dbserver') : 'server_localhost');

  // create the drush alias file for the platform
  $args = array($platform_alias);
  $options = array('root' => $platform_destination,
                   'context_type' => 'platform',
                   'web_server' => $webserver,
                   'git_platform' => $repo_info);

  provision_backend_invoke('self', 'provision-save', $args, $options);
  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", "Could not create drush-alias for platform");
  }
  drush_log(dt('Platform saved'), 'ok');

  // inform the hostmaster frontend about the new platform
  provision_backend_invoke('hostmaster', 'hosting-import', array($platform_alias));
  if (drush_get_error()) {
    return drush_set_error("HOSTING_IMPORT_FAILED", "Could not inform hostmaster of new platform");
  }
  drush_log(dt('Platform !platform imported into hostmaster', array('!platform' => $platform_alias)), 'ok');

  // have aegir process its queue
  provision_backend_invoke('hostmaster', 'hosting-dispatch');
  if (drush_get_error()) {
    return drush_set_error("HOSTING_IMPORT_FAILED", "Could get aegir to process its queue");
  }
  drush_log(dt('Aegir queue processed'), 'ok');

  // see if we need to do anything with the platform
  if(($postaction = drush_get_option('postaction')) && ($site = drush_get_option('site-hostname'))){

      $profile = drush_get_option('profile') ? drush_get_option('profile') : 'default';
      $site_alias = '@' . $site;

      if($postaction === "install"){
        // install a new site on the platform
        drush_log(dt('Installing the site !site onto platform !platform on webserver !webserver using database !dbserver',
                     array('!site' => $site, '!platform' => $platform_name, '!webserver' => $webserver, '!dbserver' => $dbserver )));

        // store the alias
        if(!_git_project_save_alias($site_alias, $site, $platform_alias, $webserver, $dbserver, $profile)){
          return FALSE;
        }
        drush_log(dt('Alias for site !site saved', array('!site' => $site)), 'ok');

        drush_log(dt('Installing site !site', array('!site' => $site)), 'ok');
        // install the site
        provision_backend_invoke($site_alias, 'provision-install');
        if (drush_get_error()) {
          return drush_set_error("SITE_INSTALL_FAILED", "Could not install the site");
        }
        drush_log(dt('Site installation successful'), 'ok');

        drush_log(dt('Verifying site to bring Hostmaster frontend in sync'));
        // queue up a verification of the platform to discover the new site
        provision_backend_invoke('hostmaster', 'hosting-task', array($platform_alias, 'verify'));
        if (drush_get_error()) {
          return drush_set_error("SITE_VERIFY_FAILED", "Could not get aegir to verify the site");
        }

        drush_log(dt('Verification successful'), 'ok');
        // done
      }

      if($postaction === "migrate"){
        drush_log(dt('Migrating the site !site onto platform !platform on webserver !webserver using database !dbserver',
                     array('!site' => $site, '!platform' => $platform_name, '!webserver' => $webserver, '!dbserver' => $dbserver )), 'ok');

        // migrate an existing site onto the platform
        $options = array('new_db_server' => $dbserver);
        provision_backend_invoke($site_alias, 'provision-migrate', array($platform_alias), $options);
        if (drush_get_error()) {
          return drush_set_error("MIGRATION_FAILED", "Could not migrate " . $site . " onto " . $platform_alias) ;
        }
        drush_log(dt('Created alias for the migrated site'), 'ok');

        // store the alias
        if(!_git_project_save_alias($site_alias, $site, $platform_alias, $webserver, $dbserver, $profile)){
          return FALSE;
        }

        // refresh frontend
        drush_log(dt('Refreshing the frontend to reflect the site under the new platform'), 'ok');
        provision_backend_invoke('hostmaster', 'hosting-import', array($site_alias));
        if (drush_get_error()) {
          return drush_set_error("HOSTING_IMPORT_FAILED", "Could not inform hostmaster of site migration");
        }

        provision_backend_invoke('hostmaster', 'hosting-task', array($platform_alias, 'verify'));
        if (drush_get_error()) {
          return drush_set_error("HOSTING_IMPORT_FAILED", "Could not verify migrated site");
        }

        provision_backend_invoke('hostmaster', 'hosting-import', array($site_alias));
        if (drush_get_error()) {
          return drush_set_error("HOSTING_IMPORT_FAILED", "Could not verify migrated site");
        }

        provision_backend_invoke('hostmaster', 'hosting-task', array($site_alias, 'verify'));
        if (drush_get_error()) {
          return drush_set_error("HOSTING_IMPORT_FAILED", "Could not verify migrated site");
        }
        drush_log(dt('Site has been verified and migrated'), 'ok');
      }
  }
}

function _git_project_save_alias($site_alias, $site_hostname, $platform_alias, $webserver, $dbserver, $profile){
  $args = array($site_alias);
  $options = array( 'context_type' => 'site',
                    'uri'          => $site_hostname,
                    'platform'     => $platform_alias,
                    'server'       => $webserver,
                    'db_server'    => $dbserver,
                    'profile'      => $profile,
                    'client_name'  => 'admin'
                  );
  provision_backend_invoke('self', 'provision-save', $args, $options);

  if (drush_get_error()) {
   return drush_set_error("ALIAS_SAVE_FAILED",
            dt("Could not save alias for site !site", array('!site' => $site_hostname))
            );
  }else{
    return TRUE;
  }

}

function _git_project_rewrite_makefile($makefile, $deploy_project, $revspec){
  // get content of makefile and parse it
  if(!($lines = file($makefile))){
    drush_set_error('MAKEFILE_NOT_FOUND', dt("Could not read makefile at @file", array('@file' => $makefile)));
    return FALSE;
  }

  $output_lines = array();

  // loop trough each source line and determine whether to a: output it unchanged, b: skip it, c: inject new lines after it
  foreach($lines as $line){
    // find lines that looks like a "type[projectname][some][keys] = value"
    if(preg_match("/^(\w+)((\[\w+\])+)\s+=\s+\"?([^\"]*)\"?/", $line, $regs)){
        $keys = $regs[2];
        $value = $regs[4];

        // get the project name
        $keys_exploded = explode(']', substr($keys, 0, -1));
        $project_name = substr($keys_exploded[0], 1);


      // TODO could do more to actually parse the line but this will for now

      // if we find a project that matches the one we're looking for ...
      if($deploy_project == $project_name){
        // skip the line if it contains any of the following
        $skip_list = array ('[download][branch]' , '[download][tag]', '[download][revision]');
        foreach($skip_list as $needle) {
          if (stripos($keys,$needle) !== false) continue 2;
        }

        // ... inject revision specification after the download-type = git line
        if(strpos($keys, '[download][type]') !== FALSE && $value == 'git'){
          // inject revision specification (should only have one set)
          foreach($revspec as $spec_name => $spec_value){
            switch($spec_name){
              case 'revision':
                $line .= "projects[$project_name][download][revision] = $spec_value\n";
                drush_print("injected revision = $spec_value into makefile");
              break;

              case 'branch':
                $line .= "projects[$project_name][download][branch] = $spec_value\n";
                drush_print("injected branch = $spec_value into makefile");
              break;

              case 'tag':
                $line .= "projects[$project_name][download][tag] = $spec_value\n";
                drush_print("injected tag = $spec_value into makefile");
              break;
            }
          }
        }
      }
    }

    // at this point the line has either been left untouched or changed - output it
    array_push($output_lines, $line);
  }

  // write the result back out
  file_put_contents($makefile, implode('', $output_lines));
  return true;
}
