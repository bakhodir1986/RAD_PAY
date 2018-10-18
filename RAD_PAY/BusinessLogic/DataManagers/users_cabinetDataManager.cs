using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class users_cabinetDataManager
    {
        //users_cabinetViewModel
//publiclongid{get;set;}
//publiclonguid{get;set;}
//publicstringpassword{get;set;}
//publicint?checkpassword{get;set;}
//publicDateTime?last_passwd_check{get;set;}
//publicint?check_count{get;set;}
//publicDateTime?checkcode_expiry{get;set;}

        public static void Add(users_cabinetViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new users_cabinet
            {   
                id                     = model.id               ,     
                uid                    = model.uid              ,
                password               = model.password         ,
                checkpassword          = model.checkpassword    ,
                last_passwd_check      = model.last_passwd_check,
                check_count            = model.check_count      ,
                checkcode_expiry       = model.checkcode_expiry ,
            };

            db.users_cabinet.Add(dbmodel);
        }

        public static void Modify(users_cabinetViewModel model, RAD_PAYEntities db)
        {
            var result = db.users_cabinet.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.id = model.id               ;     
                    dbmodel.uid = model.uid              ;
                    dbmodel.password = model.password         ;
                    dbmodel.checkpassword = model.checkpassword    ;
                    dbmodel.last_passwd_check = model.last_passwd_check;
                    dbmodel.check_count = model.check_count      ;
                    dbmodel.checkcode_expiry = model.checkcode_expiry ;
                }
            }
        }

        public static void Delete(users_cabinetViewModel model, RAD_PAYEntities db)
        {
            var result = db.users_cabinet.Where(z => z.id == model.id);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.users_cabinet.Remove(dbmodel);
                }
            }
        }

        public static List<users_cabinetViewModel> Get(users_cabinetViewModel model, RAD_PAYEntities db)
        {
            List<users_cabinetViewModel> list = null;

            var query = from resmodel in db.users_cabinet
                        select new users_cabinetViewModel
                        {
                            id = resmodel.id,
                            uid = resmodel.uid,
                            password = resmodel.password,
                            checkpassword = resmodel.checkpassword,
                            last_passwd_check = resmodel.last_passwd_check,
                            check_count = resmodel.check_count,
                            checkcode_expiry = resmodel.checkcode_expiry,
                        };

            list = query.ToList();

            return list;
        }
    }
}