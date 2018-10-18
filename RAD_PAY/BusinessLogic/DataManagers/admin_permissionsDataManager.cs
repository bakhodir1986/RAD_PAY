using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class admin_permissionsDataManager
    {
        //admin_permissionsViewModel
        //public long   aid           { get; set; }
        //public int?   modules       { get; set; }
        //public int?   merchant      { get; set; }
        //public long   bank          { get; set; }
        //public int    permit_flag    { get; set; }

        public static void Add(admin_permissionsViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new admin_permissions
            {
                aid         = model.aid         ,   
                modules     = model.modules     ,
                merchant    = model.merchant    ,
                bank        = model.bank        ,
                permit_flag = model.permit_flag ,
            };

            db.admin_permissions.Add(dbmodel);
        }

        public static void Modify(admin_permissionsViewModel model, RAD_PAYEntities db)
        {
            var result = db.admin_permissions.Where(z => z.aid == model.aid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.aid = model.aid                 ;
                    dbmodel.modules = model.modules         ;
                    dbmodel.merchant = model.merchant       ;
                    dbmodel.bank = model.bank               ;
                    dbmodel.permit_flag = model.permit_flag;
                }
            }
        }

        public static void Delete(admin_permissionsViewModel model, RAD_PAYEntities db)
        {
            var result = db.admin_permissions.Where(z => z.aid == model.aid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.admin_permissions.Remove(dbmodel);
                }
            }
        }

        public static List<admin_permissionsViewModel> Get(admin_permissionsViewModel model, RAD_PAYEntities db)
        {
            List<admin_permissionsViewModel> list = null;

            var query = from resmodel in db.admin_permissions
                        select new admin_permissionsViewModel
                        {
                            aid = resmodel.aid,
                            modules = resmodel.modules,
                            merchant = resmodel.merchant,
                            bank = resmodel.bank,
                            permit_flag = resmodel.permit_flag,
                        };

            list = query.ToList();

            return list;
        }
    }
}