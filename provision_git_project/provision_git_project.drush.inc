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


  $items['provision-git_project_deploy'] = array(
    'description' => "Builds a platform from a git repo and installs or migrates a site to it. Must be invoked in context of a deployment.",
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
      'override-platform' => array(
        'description' => 'Full path to a platform that should be copied instead of building a new from the deployments makefile',
      ),
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );


  return $items;
}

/**
 * Verify that both project and deployment exists as aliases.
 */
function drush_provision_git_project_deploy_validate() {
  // Attempt to load the deployment.
  $deployment = d();

  if ($deployment->context_type !== 'git_project_deployment') {
    return drush_set_error('INVALID_CONTEXT', dt('The command was not invoked in context of a git_project_deployment, the current context is a !type named !name',
        array(
          '!type' => $deployment->context_type,
          '!name' => $deployment->name,
        )
      ));
  }

  if (!$deployment->site) {
    // If the deployment dones not already have a reference to the site, make
    // sure the site does not already exist.
    $site_alias = _git_project_get_site_alias($deployment);

    $existing_site = provision_sitealias_get_record($site_alias);
    if ($existing_site) {
      return drush_set_error('SITE_EXISTS', dt('The site !site_alias already exist!',
        array('!site_alias' => $site_alias)));
    }
  }

  // Attempt to load the project.
  $project = d()->git_project;
  if (!$project || $project->context_type != 'git_project') {
    return drush_set_error("INVALID_CONTEXT", dt('The deployments project could not be loaded', 'error'));
  }

  $override_platform = drush_get_option('override-platform', FALSE);
  if ($override_platform !== FALSE && !is_readable($override_platform)) {
    return drush_set_error('UNREADABLE',
      dt('Could not gain read-access to the override platform !platform',
        array('!platform' => $override_platform)
      ));
  }

  // TODO halt if the user specified more than one rev spec.
}

/**
 * Deployes a new version of the project.
 *
 * Builds a new platform and either install a new site on it, or migrate an
 * existing site onto it if this is not the first deployment.
 */
function drush_provision_git_project_deploy() {
  $deployment = d();
  $project = $deployment->git_project;

  // Get revision overrides from (optional) options.
  $tag = $deployment_branch = drush_get_option('tag', FALSE);
  $sha = $deployment_branch = drush_get_option('sha', FALSE);
  $branch = $deployment_branch = drush_get_option('branch', FALSE);

  // Overrides.
  $override_platform = drush_get_option('override-platform', FALSE);

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

  // TODO, make up clever platform name, based on timestamps and/or revisions.
  $platform_alias = _git_project_get_platform_alias($deployment, $choosen_revtype, $rev);

  // Construct the platform destination path, check that it does not already
  // exist.
  $platform_destination = GIT_PROJECT_PLATFORM_ROOT . str_replace('@platform_', '', $platform_alias);
  if (file_exists($platform_destination)) {
    drush_set_error('PLATFORM_EXISTS', dt('Platform-directory "!dir" already exists', array('!dir' => $platform_destination)));
    return FALSE;
  }

  if (empty($project->project_type) || $project->project_type == 'makefile') {
     if (!drush_provision_git_project_platform_by_make($project, $rev, $choosen_revtype, $override_platform, $platform_alias, $deployment, $sha, $repo_info, $branch, $tag, $platform_destination)) {
       return FALSE;
     }
  }
  elseif ($project->project_type == 'git') {
    if (!drush_provision_git_project_platform_by_git($choosen_revtype, $rev, $project->makefile_url, $platform_destination)) {
      return FALSE;
    }
  }

  // Dump a simple version.txt.
  $version_txt_path = $platform_destination . "/version.txt";
  if (!file_exists($version_txt_path)) {
    file_put_contents($version_txt_path, 'Revision-type: ' . $choosen_revtype . "\n" . 'Value: ' . $rev);
  }

  drush_log(dt('Platform "!platform" prepared, creating alias', array('!platform' => $platform_alias)), 'ok');

  // Create the drush alias file for the platform.
  $args = array($platform_alias);
  $options = array(
    'root'         => $platform_destination,
    'context_type' => 'platform',
    'web_server'   => $project->web_server->name,
    'git_project_deployment' => $deployment->name,
  );

  drush_backend_invoke_args('provision-save', $args, $options);
  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", "Could not create drush-alias for platform");
  }
  drush_log(dt('Platform alias created, verifying platform'), 'ok');

  // Have provision verify the platform, this will also link the platform to
  // the deployment via drush_hook_post_provision_verify()
  $args = array($platform_alias);

  drush_backend_invoke_args('provision-verify', $args);
  if (drush_get_error()) {
    return drush_set_error("PROVISION_SAVE_FAILED", dt("Could not verify the platform !platform",
      array('!platform' => $platform_alias)));
  }
  drush_log(dt('Platform alias created and verfied'), 'ok');

  // Inform the hostmaster frontend about the new platform.
  drush_log('Importing platform into hostmaster', 'ok');
  provision_backend_invoke('hostmaster', 'hosting-import', array($platform_alias));
  if (drush_get_error()) {
    return drush_set_error("HOSTING_IMPORT_FAILED", "Could not inform hostmaster of new platform");
  }

  // Have aegir process its queue.
  provision_backend_invoke('hostmaster', 'hosting-dispatch');
  if (drush_get_error()) {
    return drush_set_error("HOSTING_IMPORT_FAILED", "Could get aegir to process its queue");
  }
  drush_log(dt('Import-task queued in Hostmaster'), 'ok');

  // Determine whether to install a new site, or migrate an existing.
  if (!$deployment->site) {
    // The site does not yet exist, so we have to generate the uri and alias.
    $site_alias = _git_project_get_site_alias($deployment, $project);
    $site_uri = str_replace('@', '', $site_alias);

    // Install a new site on the platform.
    drush_log(dt('Installing the site !site onto platform !platform on webserver !webserver using database !dbserver',
               array(
                 '!site'      => $site_uri,
                 '!platform'  => $platform_alias,
                 '!webserver' => $project->web_server->name,
                 '!dbserver'  => $project->db_server->name,
               )
              ), 'ok'
    );

    // Store the alias.
    if (!_git_project_save_alias($site_alias,
                                 $site_uri,
                                 $platform_alias,
                                 $project,
                                 $deployment
                                 )) {
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

    drush_log(dt('Verifying site to bring Hostmaster frontend in sync'), 'ok');
    // Queue up a verification of the platform to discover the new site.
    provision_backend_invoke('hostmaster', 'hosting-task', array($platform_alias, 'verify'));
    if (drush_get_error()) {
      return drush_set_error("SITE_VERIFY_FAILED", "Could not get aegir to verify the site");
    }

    drush_log(dt('Verification completed'), 'ok');
    // Done.
  }
  else {
    $site_alias = $deployment->site->name;
    $site_uri = $deployment->site->uri;
    // Site exists, migrate it onto the new platform.
    drush_log(dt('Migrating !site with alias !site_alias onto the new platform !platform',
               array(
                 '!site'       => $site_uri,
                 '!site_alias' => $site_alias,
                 '!platform'   => $platform_alias,
               )
              ), 'ok'
    );

    provision_backend_invoke($site_alias, 'provision-migrate', array($platform_alias));
    if (drush_get_error()) {
      return drush_set_error("MIGRATION_FAILED", "Could not migrate " .
        $site_alias . " onto " . $platform_alias);
    }

    // Refresh frontend.
    drush_log(dt('Informing Hostmaster of the migration'), 'ok');
    provision_backend_invoke('hostmaster', 'hosting-import', array($site_alias));
    if (drush_get_error()) {
      return drush_set_error("HOSTING_IMPORT_FAILED", "Could not inform hostmaster of site migration");
    }

    drush_log(dt('Site has been verified and migrated'), 'ok');
  }
}

/**
 * @param $project
 * @param $rev
 * @param $revtype
 * @param $override_platform
 * @param $platform_alias
 * @param $deployment
 * @param $sha
 * @param $repo_info
 * @param $branch
 * @param $tag
 * @param $platform_destination
 * @return bool
 */
function drush_provision_git_project_platform_by_make($project, $rev, $revtype, $override_platform, $platform_alias, $deployment, $sha, $repo_info, $branch, $tag, $platform_destination) {
// Parse the make-file url and prepare revision specific version.
  $fetch_url = _git_project_prepare_github_raw_url($project->makefile_url, $rev);
  if (!$fetch_url) {
    return drush_set_error("MAKEFILE", dt('Invalid makefile URL: Could not inject !revtype !rev into !url',
      array(
        '!url'     => $project->makefile_url,
        '!revtype' => $revtype,
        '!rev'     => $rev,
      )
    ));
  }

  // Before we start actually doing anything, confirm with the user.
  $message = "This command will\n";
  if ($override_platform === FALSE) {
    $message .= "- Create a new platform named \"$platform_alias\" using the {$revtype} \"{$rev}\"\n";
  }
  else {
    $message .= "- Create a new platform named \"$platform_alias\" by copying {$override_platform}\n";
  }


  if (!$deployment->site) {
    $message .= dt('- Install a new site using the installation profile "!profile" on the platform with the alias !sitename',
        array(
          '!profile'  => $project->install_profile,
          '!sitename' => _git_project_get_site_alias($deployment, $project)
        )
      ) . "\n";
  }
  else {
    $message .= "- Migrate the existing site \"{$deployment->site->uri}\" onto the platform\n";
  }
  $message .= "Ready to proceede?";

  if (!drush_confirm($message, 2)) {
    return;
  }
  if ($override_platform === FALSE) {
    // Download and update the makefile, then Drush make it.
    $makefile_content = file_get_contents($fetch_url);
    $makefile_path = tempnam(sys_get_temp_dir(), 'git_project_make');
    file_put_contents($makefile_path, $makefile_content);

    // Prepare revision argument for the build.
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

    // Rewrite makefile using information about the installation profile and the
    // repository.
    if (!_git_project_rewrite_makefile($makefile_path, $project->install_profile, $repo_info)) {
      return FALSE;
    }

    drush_log(dt('Building platform !platform using Drush Make', array('!platform' => $platform_alias)), 'ok');
    drush_log(dt('Using makefile !makefile', array('!makefile' => $makefile_path)), 'notice');

    // TODO: Grab "custom drush" information from a configuration.
    $custom_drush = '/var/aegir/tools/drush-5.8/drush';

    if ($custom_drush !== FALSE) {
      // Use custom Drush instead of aegir native.
      if (!drush_shell_exec("$custom_drush make %s %s", $makefile_path, $platform_destination)) {
        return drush_set_error("DRUSH_MAKE_FAILED", "Could not build platform from makefile");
      }
    }
    else {
      drush_backend_invoke_args('make', array(
        $makefile_path,
        $platform_destination
      ));
      if (drush_get_error()) {
        return drush_set_error("DRUSH_MAKE_FAILED", "Could not build platform from makefile");
      }
    }
  }
  else {
    // Override platform specified, copy it and continue.
    drush_copy_dir($override_platform, $platform_destination);
    drush_log(dt('Override platform copied from !platform_path to !platform_destination',
      array(
        '!platform_path'        => $override_platform,
        '!platform_destination' => $platform_destination,
      )), 'ok');
  }
  return TRUE;
}

/**
 * Create a platform from git.
 *
 * @param $revtype
 * @param $rev
 * @param $git_url
 * @param $platform_destination
 * @return mixed
 */
function drush_provision_git_project_platform_by_git($revtype, $rev, $git_url, $platform_destination) {
  if (!drush_shell_exec("git clone %s %s", $git_url, $platform_destination)) {
    return drush_set_error("DRUSH_MAKE_FAILED", "Could not clone repository $git_url into $platform_destination");
  }

  if (!drush_shell_exec("git --work-tree=%s --git-dir=%s checkout %s", $platform_destination, $platform_destination . "/.git", $rev)) {
    return drush_set_error("DRUSH_MAKE_FAILED", "Could not clone repository $git_url into $platform_destination");
  }

  // Produce version.txt
  $time_of_build = date('Y-m-d H:i:s');
  $sha = `git --work-tree=%s --git-dir=%s rev-parse HEAD`;
  $version_txt_content =  <<<EOT
------------
Time of build: $time_of_build
$revtype: $rev
SHA: $sha
EOT;

  file_put_contents($platform_destination . '/version.txt', $version_txt_content);

  return TRUE;
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
    'deployment_status' => 'defined',
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

  if (strtolower($project_name) === 'deployment') {
    return drush_set_error('INVALID_ARGUMENT', 'The project-name "deployment" is reserved');
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
    'install_profile' => $install_profile,
    'domain_basename' => $base_domain,
    'db_server'       => $db_server,
    'web_server'      => $web_server,
    'project_type'    => drush_get_option('project-type', 'makefile'),
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
  $out['Status']           = $p->deployment_status;
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
  $out['Install Profile'] = $p->install_profile;
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


/**
 * Implements hook_post_provision_verify().
 *
 * Bring the git project deployment context in sync when platforms and sites
 * are verified. Platforms are verified when they are installed, sites are
 * verified when the user wants to bring it back into sync.
 */
function drush_provision_git_project_post_provision_verify() {
  // Spot the deployment property.
  if (d()->git_project_deployment) {
    $deployment = d()->git_project_deployment;

    if (d()->type == 'platform' && $deployment->platform->name != d()->name) {
      drush_log("Git Project: linking this platform with the deployment " . $deployment->name, 'ok');
      $deployment->platform = d()->name;
    }
    elseif (d()->type == 'site' && $deployment->site->name != d()->name) {
      drush_log("Git Project: linking this site with the deployment " . $deployment->name, 'ok');
      $deployment->site = d()->name;
      $deployment->status = 'deployed';
    }
    else {
      // Unknown context, do nothing.
      return TRUE;
    }

    // Update the deployment drush-alias.
    $deployment->write_alias();
    if (drush_get_error()) {
      return drush_set_error("ALIAS_SAVE_FAILED",
        dt("Could not update the alias for deployment !deployment",
          array('!deployment' => $deployment->name))
      );
    }
  }
}

/**
 * Implements hook_post_provision_migrate().
 */
function drush_provision_git_project_post_provision_migrate() {
  return drush_provision_git_project_post_provision_install();
}

/**
 * Implements hook_post_provision_install().
 *
 * Bring the git project deployment context in sync when sites are installed.
 * Platforms are only verified, not installed, these are handled in
 * hook_post_provision_verify
 */
function drush_provision_git_project_post_provision_install() {
  // Spot the deployment property.
  if (d()->git_project_deployment) {
    $deployment = d()->git_project_deployment;

    // Take action if a site is being installed.
    if (d()->type == 'site') {
      if (d()->platform->git_project_deployment) {
        // Site is being installed on / migrated to a git project platform.
        drush_log("Git Project: linking this site to the deployment " . $deployment->name, 'ok');
        $deployment->site = d()->name;
        $deployment->status = 'deployed';

        if (d()->git_project_deployment->platform->name != d()->platform->name) {
          // Site is being migrated to a platform that is out of sync.
          drush_log("Git Project: updating platform reference as well");
          $deployment->platform = d()->platform->name;
        }
      }
      else {
        drush_log("Site is being migrated to a platform that does not using Git Project, breaking all relations", 'ok');
        // Site is being installed on / migrated to a platform that is not a
        // Git Project platform, break relation to the project.
        $deployment->site = '';
        $deployment->status = 'defined';
        d()->git_project_deployment = '';

        d()->write_alias();
      }

    }
    else {
      // Unknown context, do nothing.
      return TRUE;
    }

    // Update the deployment drush-alias.
    $deployment->write_alias();
    if (drush_get_error()) {
      return drush_set_error("ALIAS_SAVE_FAILED",
        dt("Could not save alias for deployment !deployment",
          array('!deployment' => $deployment->name))
      );
    }
  }
}

/**
 * Implements hook_post_provision_delete().
 */
function drush_provision_git_project_post_provision_delete() {
  // Spot the deployment property.
  if (d()->git_project_deployment) {
    $deployment = d()->git_project_deployment;

    // Take action if site or platform.
    if (d()->type == 'site') {
      drush_log("Git Project: unlinking this site from the deployment " . $deployment->name, 'ok');
      $deployment->site = '';
      $deployment->status = 'defined';
    }
    elseif (d()->type == 'platform') {
      drush_log("Git Project: unlinking this platform from the deployment " . $deployment->name, 'ok');
      $deployment->platform = '';
    }
    else {
      // Unknown context, do nothing.
      return TRUE;
    }

    // Update the deployment drush-alias.
    $deployment->write_alias();
    if (drush_get_error()) {
      return drush_set_error("ALIAS_SAVE_FAILED",
        dt("Could not save alias for deployment !deployment",
          array('!deployment' => $deployment->name))
      );
    }
  }
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
function _git_project_get_platform_alias($deployment,  $revtype, $rev) {
  // TODO, might have to be deterministic.
  return _git_project_get_alias(str_replace('.', '', $deployment->git_project->project_name) . '_' . $deployment->deployment_name . '_' . substr(strtolower($revtype),0,1) . '_' . strtolower($rev) . '_' . date("ymdHis") , 'platform_');
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
function _git_project_get_deployment_alias($deployment_short_name, $project) {
  return _git_project_get_alias($deployment_short_name, 'git_project_deployment_' . _git_project_alias_normalize($project->project_name) . '_');
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
 * Invokes provision-save to store a context into an alias.
 */
function _git_project_save_alias($site_alias, $site_hostname, $platform_alias, $project, $deployment) {
  $args = array($site_alias);
  $options = array(
    'context_type'            => 'site',
    'uri'                     => $site_hostname,
    'platform'                => $platform_alias,
    'server'                  => $project->web_server->name,
    'db_server'               => $project->db_server->name,
    'profile'                 => $project->install_profile,
    'git_project_deployment'  => $deployment->name,
    'client_name'             => 'admin',
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
 * @param string $install_profile
 *   Name of the project to look for in the makefile.
 *
 * @param array  $revspec
 *   Revision detauls.
 *
 * @return bool
 *   TRUE if successfull
 */
function _git_project_rewrite_makefile($makefile, $install_profile, $revspec) {
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
      if ($install_profile == $project_name) {

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
      array('!revinfo' => $rev_info, '!profile' => $install_profile)
      ), 'error'
    );
    return FALSE;
  }

  // Write the result back out.
  file_put_contents($makefile, implode('', $output_lines));

  return TRUE;
}


function drush_provision_git_project_pre_hosting_task(){
  $task =& drush_get_context('HOSTING_TASK');
  var_dump($task);
}
