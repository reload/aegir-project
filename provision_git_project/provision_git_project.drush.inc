<?php
/**
 * @file
 * Drush commands for supporting git-based aegir projects
 */

include_once dirname(__FILE__) . '/provision_git_project.context.service.inc';

define('GIT_PROJECT_PLATFORM_ROOT', '/var/aegir/platforms/');

/**
 * Implements hook_drush_command().
 */
function provision_git_project_drush_command() {
  $items = array();

  $items['git-project-create'] = array(
    'description' => 'Creates a project.',
    'arguments' => array(
      'project-name'    => 'Simple name for the project',
      'base-domain'     => 'Base-domain the deployments should be based on',
      'install-profile' => 'Installation profile',
      'makefile-url'    => 'Raw url for the makefile on github',
    ),
    'options' => array(
      'db_server' => array(
        'description' => 'Alias for the server that hosts the database to be used by deployments',
      ),
      'web_server' => array(
        'description' => 'Alias for the webserver that hosts the deployments',
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['git-project-deployment-define'] = array(
    'description' => "Creates/updates a deployment definition.",
    'arguments' => array(
      'project-alias' => 'Alias a existing project project',
      'deployment-name' => 'Simple name for the deployment',
    ),
    'required-arguments' => TRUE,
    'options' => array(
      'branch' => array(
        'description' => 'Make the deployment track a branch (new instances of the deployment will use branch HEAD)',
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
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
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );


  return $items;
}

/**
 * Verify that both project and deployment exists as aliases.
 */
function drush_provision_git_project_deploy_validate($project_alias = FALSE, $deployment_name = FALSE) {
  if ($project_alias === FALSE) {
    return drush_set_error('MISSING_ARGUMENT', dt('Missing: project'));
  }

  if ($deployment_name === FALSE) {
    return drush_set_error('MISSING_ARGUMENT', dt('Missing: deployment'));
  }

  // Attempt to load the project.
  $project = d(_git_project_get_project_alias($project_alias));
  if (!$project) {
    // d() has already printed an error messeage, so just return false.
    return FALSE;
  }

  // Attempt to load the project.
  $deployment = d(_git_project_get_deployment_alias($project_alias, $project->project_name));

  if (!$deployment) {
    // d() has already printed an error messeage, so just return false.
    return FALSE;
  }

  // TODO halt if the user specifeid more than one rev spec.
}

/**
 * Deployes a new version of the project.
 *
 * Builds a new platform and either install a new site on it, or migrate an
 * existing site onto it if this is not the first deployment.
 */
function drush_provision_git_project_deploy($project_name, $deployment_name) {
  // Generate alias-names and load contexts.
  $project_alias = _git_project_get_project_alias($project_name);
  $project = d($project_alias);
  $deployment_alias = _git_project_get_deployment_alias($deployment_name, $project);
  $deployment = d($deployment_alias);

  // Get revision overrides from (optional) options.
  $tag = $deployment_branch = drush_get_option('tag', FALSE);
  $sha = $deployment_branch = drush_get_option('sha', FALSE);
  $branch = $deployment_branch = drush_get_option('branch', FALSE);

  // Determine the revision info to use.
  if ($tag !== FALSE) {
    $rev = $tag;
    $choosen_revtype = 'tag';
  }
  elseif ($sha !== FALSE) {
    $rev = $sha;
    $choosen_revtype = 'SHA';
  }
  elseif ($branch !== FALSE) {
    $rev = $branch;
    $choosen_revtype = 'branch';
  }
  else {
    $rev = $deployment->deployment_branch;
    $choosen_revtype = 'branch';
  }

  // TODO, decide whether the branch should be a one-time override or whether
  // It should go back into the deploy context.
  // Parse the make-file url, put in the revision information and download
  // the file.
  $fetch_url = _git_project_prepare_github_raw_url($project->makefile_url, $rev);

  if (!$fetch_url) {
    return drush_set_error("MAKEFILE", dt('Could not fetch makefile from !url with revspec !rev',
                                           array(
                                             '!url' => $project->makefile_url,
                                             '!rev' => $rev,
                                           )
                                          ));
  }

  // Download makefile.
  // TODO, split up into functions.
  // TODO, make up clever platform name, based on timestamps and/or revisions.
  $platform_alias = _git_project_get_platform_alias($deployment);

  // Construct the platform destination path.
  $platform_destination = GIT_PROJECT_PLATFORM_ROOT . str_replace('@', '', $platform_alias);

  // TOOD: come up with an alternate name.
  if (file_exists($platform_destination)) {
    drush_set_error('PLATFORM_EXISTS', dt('Could not create platform-directory "!dir" as it already exists', array('!dir' => $platform_destination)));
    return FALSE;
  }

  // Prepare revision info.
  if ($sha !== FALSE) {
    $repo_info['revision'] = $sha;
  }
  elseif ($branch !== FALSE) {
    $repo_info['branch'] = $branch;
  }
  elseif ($tag !== FALSE) {
    $repo_info['tag'] = $tag;
  }
  else {
    // Default to deployment branch specified in master if the user did not
    // override.
    $repo_info['branch'] = $deployment->deployment_branch;
  }

  // Before we start actually doing anything, confirm with the user.
  $message  = "This command will\n";
  $message .= "- Create a new platform named \"$platform_alias\" using the {$choosen_revtype} \"{$rev}\"\n";

  if (!$deployment->site) {
    $message .= dt('- Install a new site using the installation profile "!profile" on the platform with the alias !sitename',
                  array(
                    '!profile' => $project->profile,
                    '!sitename' => _git_project_get_site_alias($deployment, $project))
                  ) .  "\n";
  }
  else {
    $message .= "- Migrate the existing site \"{$deployment->site->uri}\" onto the platform";
  }
  $message .= "Ready to proceede?";

  if (!drush_confirm($message)) {
    return;
  }

  // Fetch the make-file into a temp file.
  $makefile_content = file_get_contents($fetch_url);
  $makefile_path = tempnam(sys_get_temp_dir(), 'git_project_make');
  file_put_contents($makefile_path, $makefile_content);

  // Rewrite makefile using information about the installation profile and the
  // repository.
  if (!_git_project_rewrite_makefile($makefile_path, $project->profile, $repo_info)) {
    return FALSE;
  }

  drush_log(dt('Using @makefile to build platform', array('@platform' => $platform_alias, '@makefile' => $makefile_path)), 'ok');

  drush_backend_invoke_args('make', array($makefile_path, $platform_destination));
  if (drush_get_error()) {
    return drush_set_error("DRUSH_MAKE_FAILED", "Could not build platform from makefile");
  }

  drush_log(dt('Platform "!platform" prepared, installing it into Aegir', array('!platform' => $platform_alias), 'ok'));

  // Create the drush alias file for the platform.
  $args = array($platform_alias);
  $options = array(
    'root' => $platform_destination,
    'context_type' => 'platform',
    'web_server' => $project->web_server->name,
    'git_project_deployment' => $deployment->name);

  drush_backend_invoke_args('provision-save', $args, $options);
  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", "Could not create drush-alias for platform");
  }
  drush_log(dt('Platform saved'), 'ok');

  // Inform the hostmaster frontend about the new platform.
  provision_backend_invoke('hostmaster', 'hosting-import', array($platform_alias));
  if (drush_get_error()) {
    return drush_set_error("HOSTING_IMPORT_FAILED", "Could not inform hostmaster of new platform");
  }
  drush_log(dt('Platform !platform imported into hostmaster', array('!platform' => $platform_alias)), 'ok');

  // Have aegir process its queue.
  provision_backend_invoke('hostmaster', 'hosting-dispatch');
  if (drush_get_error()) {
    return drush_set_error("HOSTING_IMPORT_FAILED", "Could get aegir to process its queue");
  }
  drush_log(dt('Aegir queue processed'), 'ok');


  if (!$deployment->site) {
    $site_alias = _git_project_get_site_alias($deployment, $project);
    $site_uri = str_replace('@', '', $site_alias);

    // Install a new site on the platform.
    drush_log(dt('Installing the site !site onto platform !platform on webserver !webserver using database !dbserver',
               array(
                 '!site'      => $site_uri,
                 '!platform'  => $platform_alias,
                 '!webserver' => $project->web_server,
                 '!dbserver'  => $project->db_server,
               )
              )
    );

    // Store the alias.
    if (!_git_project_save_alias($site_alias,
                                 $site_uri,
                                 $platform_alias,
                                 $project->web_server->name,
                                 $project->db_server->name,
                                 $project->profile)) {
      return FALSE;
    }

    drush_log(dt('Alias for site !site saved', array('!site' => $site_uri)), 'ok');

    drush_log(dt('Installing site !site', array('!site' => $site_uri)), 'ok');

    // Install the site.
    provision_backend_invoke($site_alias, 'provision-install');
    if (drush_get_error()) {
      return drush_set_error("SITE_INSTALL_FAILED", "Could not install the site");
    }
    drush_log(dt('Site installation successful'), 'ok');

    drush_log(dt('Verifying site to bring Hostmaster frontend in sync', 'ok'));

    // Queue up a verification of the platform to discover the new site.
    provision_backend_invoke('hostmaster', 'hosting-task', array($platform_alias, 'verify'));
    if (drush_get_error()) {
      return drush_set_error("SITE_VERIFY_FAILED", "Could not get aegir to verify the site");
    }

    drush_log(dt('Verification successful'), 'ok');
    // Done.
  }
}

/**
 * Checks that the project references by the deployment exists.
 */
function drush_provision_git_project_deployment_define_validate($project_alias = FALSE, $deployment_name = FALSE) {
  if ($project_alias === FALSE) {
    return drush_set_error('MISSING_ARGUMENT', dt('Missing argument: project-alias'));
  }

  if ($deployment_name === FALSE
      || strlen(_git_project_alias_normalize($deployment_name)) == 0) {
    return drush_set_error('MISSING_ARGUMENT', dt('Missing or invalid argument: deployment-name'));
  }

  // Attempt to load the project.
  $project = d(_git_project_get_project_alias($project_alias));

  if (!$project) {
    // d() has already printed an error messeage, so just return false.
    return FALSE;
  }
}

/**
 * Defines a git project deployment by updating or creating its alias.
 */
function drush_provision_git_project_deployment_define($project_alias, $deployment_name) {
  $project_alias = _git_project_get_project_alias($project_alias);
  $project = d($project_alias);

  // Make sure the alias confirms.
  $deployment_alias = _git_project_get_deployment_alias($deployment_name, $project);

  $deployment_branch = drush_get_option('branch', 'master');

  // Prepare arguments for provision-save.
  $args = array($deployment_alias);
  $options = array(
    'context_type'      => 'git_project_deployment',
    'deployment_name'   => $deployment_name,
    'deployment_branch' => $deployment_branch,
    'status'            => 'defined',
    'git_project'       => $project_alias,
  );

  // Save the context.
  drush_backend_invoke_args('provision-save', $args, $options + array('root' => NULL, 'uri' => NULL));

  // Output result.
  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", "Could not create deployment alias");
  }
  drush_log(dt('Stored the deployment "!deployment" under the context !alias',
    array(
      '!alias' => $deployment_alias,
      '!deployment' => $deployment_name,
    )
  ), 'ok');

  echo ("\n" . _git_project_deployment_info($deployment_alias));
}

/**
 * Verifies that all arguments are passed and that the  makefile exists.
 */
function drush_provision_git_project_create_validate($project_name = FALSE,
                                                     $base_domain = FALSE,
                                                     $install_profile = FALSE,
                                                     $makefile_url = FALSE) {
  // TODO, implement remaining vaidation.
  if ($project_name === FALSE) {
    return drush_set_error('MISSING_ARGUMENT', 'missing argument: project-name');
  }

  if ($base_domain === FALSE) {
    return drush_set_error('MISSING_ARGUMENT', 'missing argument: base-domain');
  }

  if ($install_profile === FALSE) {
    return drush_set_error('MISSING_ARGUMENT', 'missing argument: install-profile');
  }

  if ($makefile_url === FALSE) {
    return drush_set_error('MISSING_ARGUMENT', 'missing argument: makefile-url');
  }

  // Validate URL.
  $makefile_content = @file_get_contents($makefile_url);
  if (!$makefile_content) {
    return drush_set_error('MISSING_MAKEFILE', 'Unable to fetch makefile from ' . $makefile_url);
  }
  if (!preg_match('/projects\[' . $install_profile . ']\[type]\s*=\s*(\'|")?profile(\'|")?/', $makefile_content)) {
    return drush_set_error('INVALID_MAKEFILE',
      'Unable to validate makefile, it did not contain the install-profile "' . $install_profile . '" ');
  }
}

/**
 * Stores a project alias.
 */
function drush_provision_git_project_create($project_name, $base_domain, $install_profile, $makefile_url) {
  // Prepare the remaining arguments ().
  $web_server = drush_get_option('web_server', '@server_master');
  $db_server = drush_get_option('db_server', '@server_localhost');

  // Normalize project name.
  $project_alias = _git_project_get_project_alias($project_name);

  // Save the context.
  $args = array($project_alias);
  $options = array(
    'context_type'    => 'git_project',
    'project_name'    => $project_name,
    'makefile_url'    => $makefile_url,
    'profile'         => $install_profile,
    'domain_basename' => $base_domain,
    'db_server'       => $db_server,
    'web_server'      => $web_server,
  );

  drush_backend_invoke_args('provision-save', $args, $options + array('root' => NULL, 'uri' => NULL));

  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", "Could not create project alias");
  }
  drush_log(dt('Stored project "!project" under the context !alias',
    array(
      '!alias' => $project_alias,
      '!project' => $project_name,
    )
    ), 'ok');
  echo "\n" . _git_project_project_info($project_alias);
}

/**
 * Pretty-prints a deployments properties.
 */
function _git_project_deployment_info($alias) {
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


/**
 * Pretty-prints a projects properties.
 */
function _git_project_project_info($alias) {
  $p = d($alias);

  $out['Project Name']    = $p->project_name;
  $out['Install Profile'] = $p->profile;
  $out['Domain Basename'] = $p->domain_basename;
  $out['DB Server']       = $p->db_server;
  $out['Web Server']      = $p->web_server;

  return _git_project_print_array($out, 'Project info');
}

/**
 * Utility function, pretty prints an array.
 */
function _git_project_print_array($array, $title = FALSE) {
  $max_len = 0;
  foreach ($array as $key => $value) {
    $max_len = $max_len < strlen($key) ? strlen($key) : $max_len;
  }
  $whitespace_count = $max_len + 1;

  $out_string = '';

  // Add title if we have one.
  if ($title !== FALSE) {
    $out_string = $title . "\n" . str_repeat('=', strlen($title)) . "\n";
  }

  // Add key/values with proper spacing.
  foreach ($array as $key => $value) {
    $out_string .= $key . str_repeat(' ', $max_len - strlen($key))
                   . ' : ' . (@strlen($value) > 0 ? $value : '<empty>') . "\n";
  }

  return ($out_string);
}

// UTILITY FUNCTIONS.
/**
 * Parses a github raw url.
 *
 * Optionallity including username/token query params and injects revision
 * information.
 */
function _git_project_prepare_github_raw_url($url, $revspec = 'master') {
  $parsed_url = parse_url($url);
  $params = array();
  if (isset($parsed_url['query'])) {
    parse_str($parsed_url['query'], $params);
  }

  // Path consists of /<user/company>/<repository>/<branch/tag/sha>/<path>.
  if (!preg_match('#^/(.*?)/(.*?)/(.*?)/(.*)#', $parsed_url['path'], $matches)) {
    return drush_set_error("INVALID_URL", "could not parse makefile-url");
  }
  list($null, $gh_user, $repo, $null, $path) = $matches;

  // Build up the url again.
  $fetch_url = $parsed_url['scheme'] . '://' . $parsed_url['host'] . '/' . $gh_user . '/' . $repo . '/' . $revspec . '/' . $path;
  $fetch_url .= '?' . $parsed_url['query'];
  return $fetch_url;
}

/**
 * Generates a platform alias.
 */
function _git_project_get_platform_alias($deployment) {
  // TODO, might have to be deterministic.
  return _git_project_get_alias($deployment->git_project->project_name . '_' . $deployment->deployment_name . '_' . time(), 'platform_');
}

/**
 * Generates a project alias on the form.
 *
 * [deployment-name].[project-domain-basename]
 */
function _git_project_get_site_alias($deployment) {
  return _git_project_get_alias($deployment->deployment_name . '.' . $deployment->git_project->domain_basename);
}


/**
 * Returns a rush alias for the project name including @.
 */
function _git_project_get_project_alias($project_alias) {
  return _git_project_get_alias($project_alias, 'git_project_');
}

/**
 * Returns drush alias for the deployment name including @.
 */
function _git_project_get_deployment_alias($deployment_alias, $project) {
  return _git_project_get_alias($deployment_alias, 'git_project_deployment_' . _git_project_alias_normalize($project->project_name) . '_');
}

/**
 * Returns normalized alias based on name. Returns $name if it starts with @.
 */
function _git_project_get_alias($name, $prefix = '') {
  return (substr($name, 0, 1) == '@') ? $name : '@' . $prefix . _git_project_alias_normalize($name);
}

/**
 * Converts the name to lowercase and strips all non-word chars except for ".-".
 */
function _git_project_alias_normalize($name) {
  return strtolower(preg_replace("/[^\w\.-]/", "", $name));
}


/**
 * Performs a checkout of a project and installs it as a platform.
 *
 * Optionally
 * migrating or installing a new site on it
 *
 * (ref https://github.com/mig5/aegir_cid/blob/master/fabfile.py)
 *
 * TODO:
 *  - switch to using proper aegir d() contexts
 */
function drush_provision_git_platform($project_name, $platform_name, $host, $repo) {
  // Get paramteres.
  $repo_info = array();
  $repo_info['url'] = $repo;

  $platform_destination = '/vars()/aegir/platforms/' . $platform_name;
  if (file_exists($platform_destination)) {
    drush_set_error('PLATFORM_EXISTS', dt('Could not create platform-directory "@dir" as it already exists', array('@dir' => $platform_destination)));
    return FALSE;
  }

  if ($revision = drush_get_option('checkout-revision')) {
    $repo_info['revision'] = $revision;
  }

  if ($branch = drush_get_option('checkout-branch')) {
    $repo_info['branch'] = $branch;
  }

  if ($branch = drush_get_option('checkout-tag')) {
    $repo_info['tag'] = $branch;
  }

  drush_log(dt('Building the platform @platform', array('@platform' => $platform_name)), 'ok');

  // If a makefile is specified, do a temp checkout, get the file, modify it and
  // feed it to drush make.
  // TODO some caching should allow us to save a checkout.
  if (!($makefile = drush_get_option('makefile'))) {
    // Plain checkout.
    if (!drush_make_download_git($project_name, $repo_info, $platform_destination)) {
      return FALSE;
    }

  }
  else {
    // Get repo into temp location.
    $temp_repo = drush_tempdir();
    if (!drush_make_download_git($project_name, $repo_info, $temp_repo)) {
      return FALSE;
    }

    // Find the make-file.
    $makefile_path = $temp_repo . '/' . $makefile;
    if (!file_exists($makefile_path)) {
      return drush_set_error('MAKEFILE_NOT_FOUND', dt('Could not find makefile "@file" in checkout', array('@file' => $makefile)));
    }

    if (!_git_project_rewrite_makefile($makefile_path, $project_name, $repo_info)) {
      return FALSE;
    }

    drush_log(dt('Using @makefile to build platform', array('@platform' => $platform_name, '@makefile' => $makefile)), 'ok');

    provision_backend_invoke('self', 'make', array($makefile_path, $platform_destination));

    if (drush_get_error()) {
      return drush_set_error("DRUSH_MAKE_FAILED", "Could not build platform from makefile");
    }
  }

  drush_log(dt('Platform "!platform" prepared, installing it into Aegir', array('!platform' => $platform_name), 'ok'));

  // Platform files are now in place, setup the aegir part.
  $platform_alias = '@platform_' . $platform_name;

  // Setup server aliases.
  $webserver = '@' . (drush_get_option('webserver') ? drush_get_option('webserver') : 'server_master');
  $dbserver = '@' . (drush_get_option('dbserver') ? drush_get_option('dbserver') : 'server_localhost');

  // Create the drush alias file for the platform.
  $args = array($platform_alias);
  $options = array(
    'root' => $platform_destination,
    'context_type' => 'platform',
    'web_server' => $webserver,
    'git_platform' => $repo_info,
  );

  provision_backend_invoke('self', 'provision-save', $args, $options);
  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", "Could not create drush-alias for platform");
  }
  drush_log(dt('Platform saved'), 'ok');

  // Inform the hostmaster frontend about the new platform.
  provision_backend_invoke('hostmaster', 'hosting-import', array($platform_alias));
  if (drush_get_error()) {
    return drush_set_error("HOSTING_IMPORT_FAILED", "Could not inform hostmaster of new platform");
  }
  drush_log(dt('Platform !platform imported into hostmaster', array('!platform' => $platform_alias)), 'ok');

  // Have aegir process its queue.
  provision_backend_invoke('hostmaster', 'hosting-dispatch');
  if (drush_get_error()) {
    return drush_set_error("HOSTING_IMPORT_FAILED", "Could get aegir to process its queue");
  }
  drush_log(dt('Aegir queue processed'), 'ok');

  // See if we need to do anything with the platform.
  if (($postaction = drush_get_option('postaction')) && ($site = drush_get_option('site-hostname'))) {

    $profile = drush_get_option('profile') ? drush_get_option('profile') : 'default';
    $site_alias = '@' . $site;

    if ($postaction === "install") {
        // install a new site on the platform
        drush_log(dt('Installing the site !site onto platform !platform on webserver !webserver using database !dbserver',
                     array('!site' => $site, '!platform' => $platform_name, '!webserver' => $webserver, '!dbserver' => $dbserver )));

        // store the alias
        if (!_git_project_save_alias($site_alias, $site, $platform_alias, $webserver, $dbserver, $profile)) {
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

      if ($postaction === "migrate") {
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
        if (!_git_project_save_alias($site_alias, $site, $platform_alias, $webserver, $dbserver, $profile)) {
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

/**
 * Invokes provision-save to store a context into an alias.
 */
function _git_project_save_alias($site_alias, $site_hostname, $platform_alias, $webserver, $dbserver, $profile) {
  $args = array($site_alias);
  $options = array(
    'context_type' => 'site',
    'uri'          => $site_hostname,
    'platform'     => $platform_alias,
    'server'       => $webserver,
    'db_server'    => $dbserver,
    'profile'      => $profile,
    'client_name'  => 'admin',
  );
  drush_backend_invoke_args('provision-save', $args, $options);

  if (drush_get_error()) {
    return drush_set_error("ALIAS_SAVE_FAILED",
            dt("Could not save alias for site !site", array('!site' => $site_hostname))
            );
  }
  else {
    return TRUE;
  }

}

/**
 * Takes a make-file and injects revision specifications into a project.
 *
 * @param string $makefile
 *   Full path to the makefile.
 *
 * @param string $profile
 *   Name of the project to look for in the makefile.
 *
 * @param array  $revspec
 *   Revision detauls.
 *
 * @return bool
 *   TRUE if successfull
 */
function _git_project_rewrite_makefile($makefile, $profile, $revspec) {
  // Get content of makefile and parse it.
  if (!($lines = file($makefile))) {
    drush_set_error('MAKEFILE_NOT_FOUND', dt("Could not read makefile at @file", array('@file' => $makefile)));
    return FALSE;
  }

  $output_lines = array();

  // Loop trough each source line and determine whether to:
  // a: output it unchanged.
  // b: skip it, c: inject new lines after it.
  $changes = 0;
  foreach ($lines as $line) {
    // Find lines that looks like a "type[projectname][some][keys] = value".
    if (preg_match("/^(\w+)((\[\w+\])+)\s+=\s+\"?([^\"]*)\"?/", $line, $regs)) {
      $keys = $regs[2];
      $value = $regs[4];

      // Get the project name.
      $keys_exploded = explode(']', substr($keys, 0, -1));
      $project_name = substr($keys_exploded[0], 1);


      // TODO could do more to actually parse the line but this will for now.
      // If we find a project that matches the one we're looking for:
      if ($profile == $project_name) {

        // Skip (ie remove) the line if it contains any of the following.
        $skip_list = array(
          '[download][branch]',
          '[download][tag]',
          '[download][revision]');

        foreach ($skip_list as $needle) {
          if (stripos($keys, $needle) !== FALSE) {
            continue 2;
          }
        }

        // Inject revision specification after the download-type = git line.
        if (strpos($keys, '[download][type]') !== FALSE && $value == 'git') {
          // Inject revision specification (should only have one set).
          foreach ($revspec as $spec_name => $spec_value) {
            switch ($spec_name) {
              case 'revision':
                $line .= "projects[$project_name][download][revision] = $spec_value\n";
                drush_log("injected revision = $spec_value into makefile", 'ok');
                $changes++;
                break;

              case 'branch':
                $line .= "projects[$project_name][download][branch] = $spec_value\n";
                drush_log("injected branch = $spec_value into makefile", 'ok');
                $changes++;
                break;

              case 'tag':
                $line .= "projects[$project_name][download][tag] = $spec_value\n";
                drush_log("injected tag = $spec_value into makefile", 'ok');
                $changes++;
                break;
            }
          }
        }
      }
    }

    // At this point the line has either been left untouched or changed.
    array_push($output_lines, $line);
  }

  if ($changes == 0) {
    // Fail if we where unable to update the makefile.
    $rev_info = array();
    foreach ($revspec as $key => $val) {
      $rev_info[] = "$key: $val";
    }
    $rev_info = implode(', ', $rev_info);

    drush_log(dt('Unable to inject revision information (!revinfo) into makefile, no insert-point for the profile "!profile" found',
      array('!revinfo' => $rev_info, '!profile' => $profile)
      ), 'error'
    );
    return FALSE;
  }

  // Write the result back out.
  file_put_contents($makefile, implode('', $output_lines));

  return TRUE;
}
